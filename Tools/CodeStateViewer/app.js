/* =============================================================
   화면 — 두 레인(C++ / Python)을 같은 조작으로 한 단계씩 진행시킨다.
   ============================================================= */
(function () {
  'use strict';

  var E = window.CSVEngine;
  var EX = window.CSVExamples;

  var LANGS = ['cpp', 'py'];
  var CUSTOM = {
    cpp: '#include <iostream>\nusing namespace std;\n\nint main()\n{\n    int x = 1;\n    cout << x << endl;\n    return 0;\n}\n',
    py: 'x = 1\nprint(x)\n'
  };

  var state = {
    tab: 0,                      // 예제 인덱스, EX.length 이면 직접 붙여넣기
    src: { cpp: '', py: '' },
    result: { cpp: null, py: null },
    idx: { cpp: 0, py: 0 },
    editing: { cpp: false, py: false }
  };

  /* ---------- 유틸 ---------- */
  function $(id) { return document.getElementById(id); }
  function esc(s) {
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  var RE = {
    cpp: /(\/\/[^\n]*)|("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')|(\b\d+\.?\d*[fF]?\b)|\b(int|double|float|bool|char|void|auto|string|return|if|else|while|for|break|continue|const|using|namespace|struct|class|true|false|static_cast|new|delete|nullptr)\b/g,
    py: /(#[^\n]*)|("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')|(\b\d+\.?\d*\b)|\b(def|return|if|elif|else|while|for|in|and|or|not|None|True|False|pass|global|break|continue|import|from|class|lambda|print|range|len|int|float|str|bool|abs)\b/g
  };

  function highlight(line, lang) {
    if (lang === 'cpp' && /^\s*#/.test(line)) return '<span class="tk-pre">' + esc(line) + '</span>';
    var re = RE[lang];
    re.lastIndex = 0;
    var out = '', last = 0, m;
    while ((m = re.exec(line)) !== null) {
      out += esc(line.slice(last, m.index));
      var cls = m[1] ? 'tk-cmt' : m[2] ? 'tk-str' : m[3] ? 'tk-num' : 'tk-kw';
      out += '<span class="' + cls + '">' + esc(m[0]) + '</span>';
      last = m.index + m[0].length;
      if (m[0].length === 0) re.lastIndex++;
    }
    out += esc(line.slice(last));
    return out;
  }

  /* ---------- 분석 ---------- */
  function analyze(lang) {
    var res;
    try {
      res = E.analyze(state.src[lang], lang);
    } catch (e) {
      res = {
        lang: lang, source: state.src[lang], lines: state.src[lang].split('\n'),
        steps: [], compileErrors: [],
        parseError: { line: 1, code: 'INTERNAL', msg: '실행기가 이 코드를 처리하지 못했습니다: ' + (e && e.message ? e.message : e) },
        truncated: false
      };
    }
    state.result[lang] = res;
    state.idx[lang] = 0;
  }

  function stepOf(lang) {
    var r = state.result[lang];
    if (!r || !r.steps.length) return null;
    return r.steps[Math.min(state.idx[lang], r.steps.length - 1)];
  }
  function prevStepOf(lang) {
    var r = state.result[lang];
    if (!r || state.idx[lang] <= 0) return null;
    return r.steps[state.idx[lang] - 1] || null;
  }

  /* ---------- 진행 ---------- */
  function move(lang, delta) {
    var r = state.result[lang];
    if (!r || !r.steps.length) return;
    var n = state.idx[lang] + delta;
    state.idx[lang] = Math.max(0, Math.min(r.steps.length - 1, n));
  }
  function moveAll(delta) {
    LANGS.forEach(function (l) { move(l, delta); });
    render();
  }
  function jumpAll(where) {
    LANGS.forEach(function (l) {
      var r = state.result[l];
      if (!r || !r.steps.length) return;
      state.idx[l] = where === 'first' ? 0 : r.steps.length - 1;
    });
    render();
  }

  /* ---------- 렌더 ---------- */
  function render() {
    LANGS.forEach(renderLane);
    var anyNext = LANGS.some(function (l) {
      var r = state.result[l];
      return r && r.steps.length && state.idx[l] < r.steps.length - 1;
    });
    var anyPrev = LANGS.some(function (l) { return state.idx[l] > 0; });
    $('btnNext').disabled = !anyNext;
    $('btnEnd').disabled = !anyNext;
    $('btnPrev').disabled = !anyPrev;
    $('btnFirst').disabled = !anyPrev;
  }

  function renderLane(lang) {
    var r = state.result[lang];
    var step = stepOf(lang);
    var lines = (r ? r.lines : []).slice();
    while (lines.length > 1 && lines[lines.length - 1].trim() === '') lines.pop();   // 끝의 빈 줄 정리

    /* 상태 / 카운터 / 진행 막대 */
    var status = $('status-' + lang), counter = $('counter-' + lang), bar = $('bar-' + lang);
    status.className = 'lane-status';
    if (!r) { status.textContent = ''; }
    else if (r.parseError) { status.textContent = '문법 오류'; status.classList.add('bad'); }
    else if (r.compileErrors.length) { status.textContent = '컴파일 실패'; status.classList.add('bad'); }
    else {
      status.textContent = (lang === 'cpp' ? '컴파일 성공 · ' : '실행 ') + r.steps.length + '단계';
      status.classList.add('ok');
    }
    var total = r ? r.steps.length : 0;
    counter.textContent = total ? (state.idx[lang] + 1) + ' / ' + total : '– / –';
    bar.style.width = total > 1 ? ((state.idx[lang] / (total - 1)) * 100) + '%' : (total ? '100%' : '0%');

    var navPrev = document.querySelector('[data-step="' + lang + '"][data-delta="-1"]');
    var navNext = document.querySelector('[data-step="' + lang + '"][data-delta="1"]');
    navPrev.disabled = !total || state.idx[lang] === 0;
    navNext.disabled = !total || state.idx[lang] >= total - 1;

    /* 코드 */
    var errLines = {};
    if (r) {
      if (r.parseError) errLines[r.parseError.line] = true;
      r.compileErrors.forEach(function (e) { errLines[e.line] = true; });
      if (step && step.error) errLines[step.error.line] = true;
    }
    var cur = step && step.line ? step.line : null;
    var html = '';
    for (var i = 0; i < lines.length; i++) {
      var no = i + 1;
      var cls = 'ln';
      if (no === cur) cls += ' current';
      if (errLines[no]) cls += ' errline';
      html += '<div class="' + cls + '" data-line="' + no + '">'
        + '<span class="num">' + no + '</span>'
        + '<span class="ptr">▶</span>'
        + '<code>' + (lines[i] === '' ? ' ' : highlight(lines[i], lang)) + '</code>'
        + '</div>';
    }
    var codeEl = $('code-' + lang);
    codeEl.innerHTML = html;
    var curEl = codeEl.querySelector('.ln.current') || codeEl.querySelector('.ln.errline');
    if (curEl) {
      var top = curEl.offsetTop, h = codeEl.clientHeight, lh = curEl.offsetHeight;
      if (top < codeEl.scrollTop + lh || top > codeEl.scrollTop + h - lh * 2) {
        codeEl.scrollTop = Math.max(0, top - h / 2 + lh);
      }
    }

    /* 컴파일/문법 오류 배너 */
    var banner = $('banner-' + lang);
    if (r && (r.parseError || r.compileErrors.length)) {
      var items = r.parseError
        ? [{ line: r.parseError.line, code: r.parseError.code, msg: r.parseError.msg }]
        : r.compileErrors;
      var why = r.parseError
        ? '이 도구가 다루는 문법 범위를 벗어났거나, 코드에 오타가 있습니다.'
        : (lang === 'cpp'
          ? '컴파일러는 위에서 아래로 읽으며 그 자리에서 이름을 확인합니다. 여기서 막혔으니 실행 파일 자체가 만들어지지 않습니다 — 단계가 0개인 이유예요.'
          : '');
      banner.innerHTML = '<div class="banner"><h3>' + (r.parseError ? '문법 오류' : '컴파일 실패 — 실행 0단계') + '</h3><ul>'
        + items.map(function (e) {
          return '<li><b>' + esc(e.line + '행' + (e.code ? ' · ' + e.code : '')) + '</b> — ' + esc(e.msg) + '</li>';
        }).join('')
        + '</ul>' + (why ? '<p class="why">' + esc(why) + '</p>' : '') + '</div>';
    } else {
      banner.innerHTML = '';
    }

    /* 설명 */
    var ex = $('explain-' + lang);
    if (!step) {
      ex.innerHTML = '<div class="note"><span class="marker endmark">실행 없음</span><span>' +
        (r && (r.compileErrors.length || r.parseError) ? '위 오류부터 해결해야 합니다.' : '코드를 넣어 주세요.') + '</span></div>';
    } else {
      var marker = step.error ? '멈춤' : step.end ? '종료' : '다음 실행';
      var mcls = step.error || step.end ? 'marker endmark' : 'marker';
      var h2 = '<div class="note"><span class="' + mcls + '">' + marker + '</span><span>'
        + esc(step.note || '') + (step.retval ? ' <span class="v-val">→ ' + esc(step.retval) + '</span>' : '') + '</span></div>';
      if (step.insight) h2 += '<div class="insight">' + esc(step.insight) + '</div>';
      if (step.error) {
        h2 += '<div class="errbox"><span class="errcode">' + esc(step.error.code || '런타임 오류')
          + (step.error.line ? ' · ' + step.error.line + '행' : '') + '</span><span>' + esc(step.error.msg) + '</span></div>';
      }
      if (r.truncated && state.idx[lang] === r.steps.length - 1) {
        h2 += '<div class="errbox"><span class="errcode">단계 한도</span><span>' + E.MAX_STEPS
          + '단계까지만 보여줍니다. 반복이 너무 길거나 끝나지 않는 코드일 수 있어요.</span></div>';
      }
      ex.innerHTML = h2;
    }

    /* 상태 패널 */
    var box = $('vars-' + lang);
    if (!step) { box.innerHTML = '<div class="empty">표시할 상태가 없습니다.</div>'; }
    else {
      var prev = prevStepOf(lang);
      var prevMap = {};
      if (prev) {
        prev.frames.forEach(function (f, fi) {
          f.vars.forEach(function (v) { prevMap[fi + ' ' + v.name] = v.display; });
        });
      }
      var out = '';
      step.frames.forEach(function (f, fi) {
        out += '<div class="frame"><div class="frame-head"><span class="frame-name">' + esc(f.title) + '</span>'
          + (f.active ? '<span class="frame-tag">현재</span>' : '') + '</div>';
        if (!f.vars.length) {
          out += '<div class="vars"><div class="empty">' +
            (lang === 'py' ? '아직 이 이름 공간에 아무것도 없습니다.' : '아직 이 스코프에 변수가 없습니다.') + '</div></div>';
        } else {
          out += '<div class="vars"><div class="vrow head"><span>이름</span><span>값</span><span>형식</span></div>';
          f.vars.forEach(function (v) {
            var key = fi + ' ' + v.name;
            var changed = prev && Object.prototype.hasOwnProperty.call(prevMap, key) && prevMap[key] !== v.display;
            var isNew = prev && !Object.prototype.hasOwnProperty.call(prevMap, key);
            var cls = 'vrow' + (changed || isNew ? ' changed' : '') + (v.state === 'garbage' ? ' garbage' : '');
            out += '<div class="' + cls + '">'
              + '<span class="v-name">' + esc(v.name)
              + (v.param ? '<span class="tagpill param">매개변수</span>' : '')
              + (v.depth ? '<span class="tagpill blk">블록 ' + v.depth + '</span>' : '')
              + '</span>'
              + '<span class="v-val">' + esc(v.display)
              + (v.badge ? '<span class="tagpill">' + esc(v.badge) + '</span>' : '') + '</span>'
              + '<span class="v-type">' + esc(v.type || '') + '</span>'
              + '</div>';
          });
          out += '</div>';
        }
        out += '</div>';
      });
      box.innerHTML = out;
    }

    /* 출력 */
    $('out-' + lang).textContent = step ? step.out : '';
  }

  /* ---------- 탭 ---------- */
  function renderTabs() {
    var nav = $('tabs');
    var html = EX.map(function (ex, i) {
      return '<button class="tab" role="tab" type="button" data-tab="' + i + '" aria-selected="'
        + (state.tab === i) + '">' + esc(ex.title) + '</button>';
    }).join('');
    html += '<button class="tab custom" role="tab" type="button" data-tab="' + EX.length + '" aria-selected="'
      + (state.tab === EX.length) + '">+ 직접 붙여넣기</button>';
    nav.innerHTML = html;
  }

  function loadTab(i) {
    state.tab = i;
    LANGS.forEach(function (l) { state.editing[l] = false; });
    if (i < EX.length) {
      var ex = EX[i];
      state.src.cpp = ex.cpp;
      state.src.py = ex.py;
      $('brTitle').textContent = ex.title;
      $('brLede').textContent = ex.lede;
      $('brPoint').textContent = ex.point;
    } else {
      state.src.cpp = CUSTOM.cpp;
      state.src.py = CUSTOM.py;
      $('brTitle').textContent = '직접 붙여넣기';
      $('brLede').textContent = '각 레인의 편집 버튼을 눌러 코드를 넣고, 다시 실행을 누르면 그 코드로 한 줄씩 진행합니다.';
      $('brPoint').textContent = '두 쪽을 꼭 같은 코드로 맞출 필요는 없습니다. 한쪽만 바꿔서 비교해도 됩니다.';
    }
    LANGS.forEach(function (l) {
      $('edit-' + l).value = state.src[l];
      setEditing(l, false, true);
      analyze(l);
    });
    renderTabs();
    render();
  }

  /* ---------- 편집 ---------- */
  function setEditing(lang, on, silent) {
    state.editing[lang] = on;
    $('edit-' + lang).classList.toggle('hidden', !on);
    $('code-' + lang).classList.toggle('hidden', on);
    var btn = document.querySelector('[data-edit="' + lang + '"]');
    btn.textContent = on ? '실행' : '편집';
    btn.classList.toggle('wide', true);
    if (on) {
      $('edit-' + lang).value = state.src[lang];
      $('edit-' + lang).focus();
    } else if (!silent) {
      state.src[lang] = $('edit-' + lang).value;
      analyze(lang);
      render();
    }
  }

  /* ---------- 테마 ---------- */
  function readTheme() {
    try { return localStorage.getItem('csv-theme') || ''; } catch (e) { return ''; }
  }
  function applyTheme(t) {
    if (t) document.documentElement.setAttribute('data-theme', t);
    else document.documentElement.removeAttribute('data-theme');
    try { if (t) localStorage.setItem('csv-theme', t); else localStorage.removeItem('csv-theme'); } catch (e) { /* 저장 불가 환경 */ }
  }
  function toggleTheme() {
    var now = document.documentElement.getAttribute('data-theme');
    if (!now) {
      var dark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
      applyTheme(dark ? 'light' : 'dark');
    } else {
      applyTheme(now === 'dark' ? 'light' : 'dark');
    }
  }

  /* ---------- 이벤트 ---------- */
  function bind() {
    $('tabs').addEventListener('click', function (e) {
      var b = e.target.closest('[data-tab]');
      if (b) loadTab(parseInt(b.getAttribute('data-tab'), 10));
    });
    $('btnNext').addEventListener('click', function () { moveAll(1); });
    $('btnPrev').addEventListener('click', function () { moveAll(-1); });
    $('btnFirst').addEventListener('click', function () { jumpAll('first'); });
    $('btnEnd').addEventListener('click', function () { jumpAll('end'); });
    $('themeBtn').addEventListener('click', toggleTheme);

    document.addEventListener('click', function (e) {
      var s = e.target.closest('[data-step]');
      if (s) {
        move(s.getAttribute('data-step'), parseInt(s.getAttribute('data-delta'), 10));
        render();
        return;
      }
      var ed = e.target.closest('[data-edit]');
      if (ed) {
        var lang = ed.getAttribute('data-edit');
        setEditing(lang, !state.editing[lang]);
      }
    });

    // 코드 줄을 누르면 그 줄에 멈추는 단계로 이동 (중단점처럼)
    LANGS.forEach(function (lang) {
      $('code-' + lang).addEventListener('click', function (e) {
        var row = e.target.closest('.ln');
        if (!row) return;
        var line = parseInt(row.getAttribute('data-line'), 10);
        var r = state.result[lang];
        if (!r || !r.steps.length) return;
        var from = state.idx[lang];
        for (var k = 1; k <= r.steps.length; k++) {
          var j = (from + k) % r.steps.length;
          if (r.steps[j].line === line) { state.idx[lang] = j; render(); return; }
        }
      });
    });

    document.addEventListener('keydown', function (e) {
      var tag = (e.target.tagName || '').toLowerCase();
      if (tag === 'textarea' || tag === 'input') return;
      if (e.key === 'ArrowRight') { e.preventDefault(); moveAll(1); }
      else if (e.key === 'ArrowLeft') { e.preventDefault(); moveAll(-1); }
      else if (e.key === 'Home') { e.preventDefault(); jumpAll('first'); }
      else if (e.key === 'End') { e.preventDefault(); jumpAll('end'); }
    });
  }

  /* ---------- 시작 ---------- */
  applyTheme(readTheme());
  renderTabs();
  bind();
  loadTab(0);
})();
