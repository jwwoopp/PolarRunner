#include "Sound.h"
#include <xaudio2.h>
#include <objbase.h>
#include <fstream>
#include <cstring>
#include <cassert>
#include <Windows.h>

namespace
{
	// [임시 디버그] 게임플레이 중 사운드가 왜 안 나는지 추적하기 위한 파일 로그.
	// 콘솔은 렌더 타겟이라 못 쓰고, DebugView도 없는 환경이라 파일로 남긴다.
	// 원인 확인 끝나면 제거할 것.
	static void DebugLog(const std::string& message)
	{
		std::ofstream log("SoundDebug.log", std::ios::app);
		if (log.is_open())
		{
			log << message << "\n";
		}
	}

	// WAV 청크의 데이터 시작 오프셋/크기를 담는 구조체.
	struct WavChunk
	{
		size_t offset = 0;
		size_t size = 0;
	};

	// 리틀 엔디안 16비트 정수 읽기 함수.
	static bool ReadU16(const std::vector<char>& data, size_t offset, std::uint16_t& out)
	{
		// 범위 검사.
		if (offset + 2 > data.size())
		{
			return false;
		}

		out = static_cast<std::uint8_t>(data[offset])
			| (static_cast<std::uint8_t>(data[offset + 1]) << 8);
		return true;
	}

	// 리틀 엔디안 32비트 정수 읽기 함수.
	static bool ReadU32(const std::vector<char>& data, size_t offset, std::uint32_t& out)
	{
		// 범위 검사.
		if (offset + 4 > data.size())
		{
			return false;
		}

		out = static_cast<std::uint8_t>(data[offset])
			| (static_cast<std::uint8_t>(data[offset + 1]) << 8)
			| (static_cast<std::uint8_t>(data[offset + 2]) << 16)
			| (static_cast<std::uint8_t>(data[offset + 3]) << 24);
		return true;
	}

	static bool ParseWavChunk(
		const std::vector<char>& data,
		const char* chunkName,
		WavChunk& chunk)
	{
		// RIFF 헤더 이후부터 청크를 순회.
		size_t offset = 12;
		while (offset + 8 <= data.size())
		{
			// [4바이트 청크 ID][4바이트 청크 크기]
			const char* chunkId = data.data() + offset;
			std::uint32_t chunkSize = 0;
			if (!ReadU32(data, offset + 4, chunkSize))
			{
				return false;
			}

			// 실제 청크 데이터 시작 위치.
			const size_t chunkDataOffset = offset + 8;
			if (chunkDataOffset + chunkSize > data.size())
			{
				return false;
			}

			// 요청한 청크 이름과 일치하면 즉시 반환.
			if (std::strncmp(chunkId, chunkName, 4) == 0)
			{
				chunk.offset = chunkDataOffset;
				chunk.size = chunkSize;
				return true;
			}

			// WAV 청크는 2바이트 정렬을 사용하므로 패딩 반영.
			const size_t paddedSize = (chunkSize + 1) & ~1u;
			offset = chunkDataOffset + paddedSize;
		}

		return false;
	}

	static bool ParseWavData(
		const std::vector<char>& rawData,
		std::vector<char>& outWaveFormatData,
		std::vector<std::uint8_t>& outPcmData)
	{
		// 최소 헤더 크기 검사.
		if (rawData.size() < 44)
		{
			return false;
		}

		// RIFF/WAVE 파일인지 식별자 검사.
		if (std::strncmp(rawData.data(), "RIFF", 4) != 0)
		{
			return false;
		}

		if (std::strncmp(rawData.data() + 8, "WAVE", 4) != 0)
		{
			return false;
		}

		// 필수 청크(fmt/data) 위치 찾기.
		WavChunk formatChunk = {};
		WavChunk dataChunk = {};
		if (!ParseWavChunk(rawData, "fmt ", formatChunk))
		{
			return false;
		}

		if (!ParseWavChunk(rawData, "data", dataChunk))
		{
			return false;
		}

		// fmt 청크 핵심 필드 읽기.
		std::uint16_t audioFormat = 0;
		std::uint16_t numChannels = 0;
		std::uint32_t sampleRate = 0;
		std::uint16_t bitsPerSample = 0;
		if (!ReadU16(rawData, formatChunk.offset + 0, audioFormat))
		{
			return false;
		}
		if (!ReadU16(rawData, formatChunk.offset + 2, numChannels))
		{
			return false;
		}
		if (!ReadU32(rawData, formatChunk.offset + 4, sampleRate))
		{
			return false;
		}
		if (!ReadU16(rawData, formatChunk.offset + 14, bitsPerSample))
		{
			return false;
		}

		// 현재 구현은 PCM만 지원.
		if (audioFormat != WAVE_FORMAT_PCM || numChannels == 0 || sampleRate == 0 || bitsPerSample == 0)
		{
			return false;
		}

		// XAudio2 CreateSourceVoice에 넘길 WAVEFORMATEX 생성.
		outWaveFormatData.resize(sizeof(WAVEFORMATEX), 0);
		WAVEFORMATEX* format = reinterpret_cast<WAVEFORMATEX*>(outWaveFormatData.data());
		format->wFormatTag = WAVE_FORMAT_PCM;
		format->nChannels = numChannels;
		format->nSamplesPerSec = sampleRate;
		format->wBitsPerSample = bitsPerSample;
		format->nBlockAlign = static_cast<WORD>((numChannels * bitsPerSample) / 8);
		format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
		format->cbSize = 0;

		// PCM 샘플 데이터 복사.
		outPcmData.resize(dataChunk.size, 0);
		std::memcpy(
			outPcmData.data(),
			rawData.data() + dataChunk.offset,
			dataChunk.size
		);

		return true;
	}
}

// 전역 변수 초기화.
Sound* Sound::instance = nullptr;

Sound::Sound()
{
	// 싱글톤 변수 설정.
	assert(!instance);
	instance = this;

	// XAudio2 사용 전, 현재 스레드 COM 초기화.
	// 멀티스레드 아파트 모델(COINIT_MULTITHREADED) 사용.
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr))
	{
		// S_OK: 이번에 초기화됨.
		// S_FALSE: 이미 초기화됨(동일 스레드).
		comInitializedBySound = (hr == S_OK || hr == S_FALSE);
	}
	else if (hr != RPC_E_CHANGED_MODE)
	{
		// COM 초기화 자체가 불가능한 경우 사운드 시스템 사용 불가.
		return;
	}

	// XAudio2 엔진 객체 생성.
	hr = XAudio2Create(&xaudio2);
	if (FAILED(hr))
	{
		DebugLog("[Sound] XAudio2Create 실패 hr=" + std::to_string(hr));
		xaudio2 = nullptr;
		return;
	}

	// 실제 출력 장치(스피커)와 연결되는 마스터 보이스 생성.
	hr = xaudio2->CreateMasteringVoice(&masteringVoice);
	if (FAILED(hr))
	{
		DebugLog("[Sound] CreateMasteringVoice 실패 hr=" + std::to_string(hr));
		masteringVoice = nullptr;
		xaudio2->Release();
		xaudio2 = nullptr;
		return;
	}

	DebugLog("[Sound] 초기화 성공");
}

Sound::~Sound()
{
	// 배경 음악 보이스 먼저 정리.
	StopBGM();

	// 재생 중이거나 대기 중인 원샷 보이스 모두 정리.
	for (IXAudio2SourceVoice* voice : activeOneShotVoices)
	{
		if (voice)
		{
			voice->DestroyVoice();
		}
	}
	activeOneShotVoices.clear();

	// 마스터 보이스 정리.
	if (masteringVoice)
	{
		masteringVoice->DestroyVoice();
		masteringVoice = nullptr;
	}

	// XAudio2 엔진 정리.
	if (xaudio2)
	{
		xaudio2->Release();
		xaudio2 = nullptr;
	}

	// 생성자에서 COM 초기화를 수행한 경우에만 해제.
	if (comInitializedBySound)
	{
		CoUninitialize();
		comInitializedBySound = false;
	}
}

Sound& Sound::Get()
{
	assert(instance);
	return *instance;
}

void Sound::PlayOneShot(const std::string& path)
{
	DebugLog("[Sound] PlayOneShot 호출: " + path);

	// 사운드 엔진 초기화 실패 시 무시.
	if (!xaudio2)
	{
		DebugLog("[Sound] PlayOneShot 중단: xaudio2 null");
		return;
	}

	// 요청된 파일을 캐시에 로드.
	LoadSoundEffect(path);
	auto found = soundCache.find(path);
	if (found == soundCache.end())
	{
		DebugLog("[Sound] PlayOneShot 중단: 캐시에 없음(로드 실패) " + path);
		return;
	}

	// 이전에 끝난 보이스를 먼저 정리해 누적 방지.
	CleanupStoppedVoices();

	const CachedSound& cachedSound = found->second;
	const WAVEFORMATEX* format = reinterpret_cast<const WAVEFORMATEX*>(cachedSound.waveFormatData.data());
	if (!format)
	{
		DebugLog("[Sound] PlayOneShot 중단: format null " + path);
		return;
	}

	// 원샷 호출마다 SourceVoice를 새로 생성해서
	// 동일 파일도 동시에 여러 번 재생 가능하도록 처리.
	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT hr = xaudio2->CreateSourceVoice(&sourceVoice, format);
	if (FAILED(hr) || !sourceVoice)
	{
		DebugLog("[Sound] PlayOneShot 중단: CreateSourceVoice 실패 hr="
			+ std::to_string(hr) + " " + path);
		return;
	}

	// 한 번 재생할 PCM 버퍼 설정.
	XAUDIO2_BUFFER buffer = {};
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.AudioBytes = static_cast<UINT32>(cachedSound.pcmData.size());
	buffer.pAudioData = cachedSound.pcmData.data();

	hr = sourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
	{
		DebugLog("[Sound] PlayOneShot 중단: SubmitSourceBuffer 실패 hr="
			+ std::to_string(hr) + " " + path);
		sourceVoice->DestroyVoice();
		return;
	}

	// 재생 시작.
	hr = sourceVoice->Start();
	if (FAILED(hr))
	{
		DebugLog("[Sound] PlayOneShot 중단: Start 실패 hr="
			+ std::to_string(hr) + " " + path);
		sourceVoice->DestroyVoice();
		return;
	}

	DebugLog("[Sound] PlayOneShot 재생 시작 성공: " + path);

	// 재생 중 보이스 목록에 등록(종료 시 Cleanup에서 정리).
	activeOneShotVoices.emplace_back(sourceVoice);
}

void Sound::PlayBGM(const std::string& path, bool loop)
{
	DebugLog("[Sound] PlayBGM 호출: " + path);

	// 사운드 엔진 초기화 실패 시 무시.
	if (!xaudio2)
	{
		DebugLog("[Sound] PlayBGM 중단: xaudio2 null");
		return;
	}

	// 동일 BGM이 이미 재생 중이면 중복 호출 무시.
	if (currentMusic == path && backgroundVoice)
	{
		return;
	}

	// 요청된 BGM 파일 캐시 로드.
	LoadSoundEffect(path);
	auto found = soundCache.find(path);
	if (found == soundCache.end())
	{
		return;
	}

	// 기존 BGM 보이스가 있으면 먼저 중지/파괴.
	StopBGM();

	const CachedSound& cachedSound = found->second;
	const WAVEFORMATEX* format = reinterpret_cast<const WAVEFORMATEX*>(cachedSound.waveFormatData.data());
	if (!format)
	{
		return;
	}

	// BGM은 단일 SourceVoice를 유지.
	HRESULT hr = xaudio2->CreateSourceVoice(&backgroundVoice, format);
	if (FAILED(hr) || !backgroundVoice)
	{
		backgroundVoice = nullptr;
		return;
	}

	// 요청에 따라 무한 반복 또는 단발 재생 버퍼 설정.
	XAUDIO2_BUFFER buffer = {};
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE:
	buffer.AudioBytes = static_cast<UINT32>(cachedSound.pcmData.size());
	buffer.pAudioData = cachedSound.pcmData.data();

	hr = backgroundVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
	{
		backgroundVoice->DestroyVoice();
		backgroundVoice = nullptr;
		return;
	}

	// BGM 재생 시작.
	hr = backgroundVoice->Start();
	if (FAILED(hr))
	{
		backgroundVoice->DestroyVoice();
		backgroundVoice = nullptr;
		return;
	}

	// 현재 재생중 BGM 경로 기록.
	currentMusic = path;
	DebugLog("[Sound] PlayBGM 재생 시작 성공: " + path);
}

void Sound::StopBGM()
{
	// 활성 BGM 보이스가 있을 때만 정지/정리.
	if (backgroundVoice)
	{
		backgroundVoice->Stop();
		backgroundVoice->FlushSourceBuffers();
		backgroundVoice->DestroyVoice();
		backgroundVoice = nullptr;
	}

	// 현재 BGM 상태 초기화.
	currentMusic.clear();
}

void Sound::LoadSoundEffect(const std::string& filename)
{
	// 이미 로드된 파일이면 캐시 재사용.
	if (soundCache.find(filename) != soundCache.end())
	{
		return;
	}

	// WAV 파일 바이너리 읽기.
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open())
	{
		// 합쳐진 전체 경로를 그대로 찍어서 경로/폴더명 오타를 바로 확인할 수 있게 한다.
		// std::cerr/printf는 쓰지 않는다 — 콘솔 화면이 렌더 타겟(ScreenBuffer)이라 출력이 화면을 깨뜨린다.
		DebugLog("[Sound] 로드 실패(파일 열기 실패): " + filename);
		return;
	}

	std::vector<char> rawData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (rawData.empty())
	{
		DebugLog("[Sound] 로드 실패(빈 파일): " + filename);
		return;
	}

	// WAV 파싱 후 캐시에 저장.
	CachedSound cachedSound = {};
	if (!ParseWavData(rawData, cachedSound.waveFormatData, cachedSound.pcmData))
	{
		DebugLog("[Sound] 로드 실패(WAV 파싱 실패, PCM 아니거나 손상됨): " + filename);
		return;
	}

	DebugLog("[Sound] 로드 성공: " + filename + " size=" + std::to_string(rawData.size()));
	soundCache.insert({ filename, std::move(cachedSound) });
}

void Sound::CleanupStoppedVoices()
{
	// 정리할 보이스가 없으면 즉시 종료.
	if (activeOneShotVoices.empty())
	{
		return;
	}

	// erase를 수행하므로 뒤에서 앞으로 순회.
	for (int ix = static_cast<int>(activeOneShotVoices.size()) - 1; ix >= 0; --ix)
	{
		IXAudio2SourceVoice* voice = activeOneShotVoices[ix];
		if (!voice)
		{
			activeOneShotVoices.erase(activeOneShotVoices.begin() + ix);
			continue;
		}

		// 큐에 남은 버퍼가 있으면 아직 재생 중으로 판단.
		XAUDIO2_VOICE_STATE state = {};
		voice->GetState(&state);
		if (state.BuffersQueued > 0)
		{
			continue;
		}

		// 재생 종료된 보이스 파괴 후 목록에서 제거.
		voice->DestroyVoice();
		activeOneShotVoices.erase(activeOneShotVoices.begin() + ix);
	}
}
