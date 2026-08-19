#pragma once
#pragma warning(disable: 4251)

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

#if BUILD_DLL
#define SOUND_SYSTEM_API DLLEXPORT
#else
#define SOUND_SYSTEM_API DLLIMPORT
#endif

class SOUND_SYSTEM_API Sound
{
	// WAV 파일에서 파싱해둔 재생 데이터 묶음.
	// waveFormatData: WAVEFORMATEX 메모리 블록.
	// pcmData       : PCM 샘플 바이트 배열.
	struct CachedSound
	{
		std::vector<char> waveFormatData;
		std::vector<std::uint8_t> pcmData;
	};

public:
	// 생성자에서 COM/XAudio2 엔진 리소스 초기화.
	Sound();
	// 소멸자에서 보이스/엔진/COM 리소스 정리.
	~Sound();

	// 싱글톤 접근 함수.
	static Sound& Get();

	// 단순 오디오 애셋 재생 함수(사운드 효과 등).
	// 겹침 재생 가능.
	void PlayOneShot(const std::string& filename);

	// 오디오를 배경 음악으로 재생할 때 사용할 함수.
	void PlayBGM(const std::string& filename, bool loop);

	// 재생 중인 BGM 중지 함수.
	void StopBGM();

private:
	// 파일 경로 기준으로 효과음을 캐시에 로드.
	// 이미 로드된 파일이면 재로딩하지 않음.
	void LoadSoundEffect(const std::string& filename);

	// 재생이 끝난 원샷 보이스를 정리해 메모리 누수 방지.
	void CleanupStoppedVoices();

private:
	// XAudio2 엔진 핸들.
	struct IXAudio2* xaudio2 = nullptr;

	// 기본 출력 장치 마스터 보이스.
	struct IXAudio2MasteringVoice* masteringVoice = nullptr;

	// 동시 재생 중인 효과음 보이스 목록.
	std::vector<struct IXAudio2SourceVoice*> activeOneShotVoices;

	// 현재 재생 중인 배경 음악 보이스.
	struct IXAudio2SourceVoice* backgroundVoice = nullptr;

	// 현재 재생 중인 배경 음악 파일 이름.
	std::string currentMusic;

	// 로드한 효과음을 파일 경로 기준으로 캐시하는 맵.
	std::unordered_map<std::string, CachedSound> soundCache;

	// 현재 스레드에서 COM 초기화를 직접 수행했는지 여부.
	bool comInitializedBySound = false;

	// 싱글톤 구현을 위한 전역 변수.
	static Sound* instance;
};