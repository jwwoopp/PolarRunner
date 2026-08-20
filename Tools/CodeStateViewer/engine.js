/* =============================================================
   Step Engine — C++/Python 부분집합 한 줄 실행기
   전역 객체 CSVEngine 를 노출한다.
   - lex → parse → (C++만) 선언순서 정적검사 → 제너레이터 실행 → 스냅샷 배열
   ============================================================= */
(function (root) {
  'use strict';

  var MAX_STEPS = 2500;

  /* ---------- 오류 ---------- */
  function CompileErr(line, code, msg) { this.line = line; this.code = code; this.msg = msg; this.__ce = true; }
  function RuntimeErr(line, code, msg) { this.line = line; this.code = code; this.msg = msg; this.__rt = true; }
  function ReturnSig(value) { this.value = value; this.__ret = true; }

  function ce(line, code, msg) { return new CompileErr(line, code, msg); }
  function rt(line, code, msg) { return new RuntimeErr(line, code, msg); }

  /* =============================================================
     1. 렉서
     ============================================================= */
  var OPS2 = ['<<', '>>', '++', '--', '+=', '-=', '*=', '/=', '%=', '==', '!=', '<=', '>=', '&&', '||', '//', '**'];
  var OPS1 = '+-*/%=<>!(){}[];,.:&|';

  function unescapeChar(c) {
    if (c === 'n') return '\n';
    if (c === 't') return '\t';
    if (c === '0') return '\0';
    if (c === '\\') return '\\';
    return c;
  }

  function lexCpp(src) {
    var toks = [], i = 0, line = 1, n = src.length;
    function push(k, v) { toks.push({ k: k, v: v, line: line }); }
    while (i < n) {
      var c = src[i];
      if (c === '\n') { line++; i++; continue; }
      if (c === ' ' || c === '\t' || c === '\r') { i++; continue; }
      if (c === '#') { while (i < n && src[i] !== '\n') i++; continue; }              // 전처리기: 통째로 무시
      if (c === '/' && src[i + 1] === '/') { while (i < n && src[i] !== '\n') i++; continue; }
      if (c === '/' && src[i + 1] === '*') {
        i += 2;
        while (i < n && !(src[i] === '*' && src[i + 1] === '/')) { if (src[i] === '\n') line++; i++; }
        i += 2; continue;
      }
      if (c === '"' || c === "'") {
        var q = c, j = i + 1, s = '';
        while (j < n && src[j] !== q) {
          if (src[j] === '\\') { s += unescapeChar(src[j + 1]); j += 2; }
          else { s += src[j]; j++; }
        }
        push('STR', s); i = j + 1; continue;
      }
      if (c >= '0' && c <= '9') {
        var j2 = i, isF = false;
        while (j2 < n && src[j2] >= '0' && src[j2] <= '9') j2++;
        if (src[j2] === '.' && src[j2 + 1] >= '0' && src[j2 + 1] <= '9') {
          isF = true; j2++;
          while (j2 < n && src[j2] >= '0' && src[j2] <= '9') j2++;
        }
        if (src[j2] === 'f' || src[j2] === 'F') { isF = true; j2++; }
        push('NUM', { v: parseFloat(src.slice(i, j2)), f: isF }); i = j2; continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        var j3 = i;
        while (j3 < n && /[A-Za-z0-9_]/.test(src[j3])) j3++;
        var name = src.slice(i, j3); i = j3;
        while (src[i] === ':' && src[i + 1] === ':' && /[A-Za-z_]/.test(src[i + 2] || '')) {
          var j4 = i + 2;
          while (j4 < n && /[A-Za-z0-9_]/.test(src[j4])) j4++;
          name += '::' + src.slice(i + 2, j4); i = j4;
        }
        push('NAME', name); continue;
      }
      var two = src.substr(i, 2);
      if (two !== '//' && OPS2.indexOf(two) >= 0) { push('OP', two); i += 2; continue; }
      if (OPS1.indexOf(c) >= 0) { push('OP', c); i++; continue; }
      throw ce(line, 'C2018', "알 수 없는 문자 '" + c + "' 입니다.");
    }
    toks.push({ k: 'EOF', v: null, line: line });
    return toks;
  }

  function lexPy(src) {
    var lines = src.replace(/\r\n?/g, '\n').split('\n');
    var toks = [], indents = [0], depth = 0, lastLine = 1;
    for (var li = 0; li < lines.length; li++) {
      var lineNo = li + 1;
      var text = lines[li].replace(/\t/g, '    ');
      lastLine = lineNo;

      if (depth === 0) {
        var ind = /^ */.exec(text)[0].length;
        var body = text.slice(ind);
        if (body.trim() === '' || body.trim()[0] === '#') continue;
        if (ind > indents[indents.length - 1]) {
          indents.push(ind);
          toks.push({ k: 'INDENT', v: null, line: lineNo });
        } else {
          while (ind < indents[indents.length - 1]) {
            indents.pop();
            toks.push({ k: 'DEDENT', v: null, line: lineNo });
          }
          if (ind !== indents[indents.length - 1]) {
            throw ce(lineNo, 'IndentationError', '들여쓰기 칸 수가 앞의 블록과 맞지 않습니다.');
          }
        }
      }

      var i = 0, n = text.length, emitted = 0;
      while (i < n) {
        var c = text[i];
        if (c === ' ' || c === '\r') { i++; continue; }
        if (c === '#') break;
        if (c === '"' || c === "'") {
          var q = c, j = i + 1, s = '';
          while (j < n && text[j] !== q) {
            if (text[j] === '\\') { s += unescapeChar(text[j + 1]); j += 2; }
            else { s += text[j]; j++; }
          }
          if (j >= n) throw ce(lineNo, 'SyntaxError', '따옴표가 닫히지 않았습니다.');
          toks.push({ k: 'STR', v: s, line: lineNo }); emitted++; i = j + 1; continue;
        }
        if (c >= '0' && c <= '9') {
          var j2 = i, isF = false;
          while (j2 < n && text[j2] >= '0' && text[j2] <= '9') j2++;
          if (text[j2] === '.' && text[j2 + 1] >= '0' && text[j2 + 1] <= '9') {
            isF = true; j2++;
            while (j2 < n && text[j2] >= '0' && text[j2] <= '9') j2++;
          }
          toks.push({ k: 'NUM', v: { v: parseFloat(text.slice(i, j2)), f: isF }, line: lineNo });
          emitted++; i = j2; continue;
        }
        if (/[A-Za-z_]/.test(c)) {
          var j3 = i;
          while (j3 < n && /[A-Za-z0-9_]/.test(text[j3])) j3++;
          toks.push({ k: 'NAME', v: text.slice(i, j3), line: lineNo });
          emitted++; i = j3; continue;
        }
        var two = text.substr(i, 2);
        if (OPS2.indexOf(two) >= 0 && two !== '<<' && two !== '>>' && two !== '&&' && two !== '||') {
          toks.push({ k: 'OP', v: two, line: lineNo }); emitted++; i += 2; continue;
        }
        if (OPS1.indexOf(c) >= 0) {
          if (c === '(' || c === '[' || c === '{') depth++;
          if (c === ')' || c === ']' || c === '}') depth = Math.max(0, depth - 1);
          toks.push({ k: 'OP', v: c, line: lineNo }); emitted++; i++; continue;
        }
        throw ce(lineNo, 'SyntaxError', "알 수 없는 문자 '" + c + "' 입니다.");
      }
      if (emitted > 0 && depth === 0) toks.push({ k: 'NEWLINE', v: null, line: lineNo });
    }
    while (indents.length > 1) { indents.pop(); toks.push({ k: 'DEDENT', v: null, line: lastLine + 1 }); }
    toks.push({ k: 'EOF', v: null, line: lastLine + 1 });
    return toks;
  }

  /* =============================================================
     2. 파서
     ============================================================= */
  var CPP_TYPES = ['int', 'double', 'float', 'bool', 'char', 'void', 'auto', 'string', 'std::string', 'long', 'short'];
  var PY_KEYWORDS = ['if', 'elif', 'else', 'while', 'for', 'def', 'return', 'pass', 'global', 'in', 'and', 'or', 'not', 'True', 'False', 'None', 'break', 'continue'];

  function Parser(toks, lang) {
    this.t = toks; this.p = 0; this.lang = lang;
  }
  Parser.prototype.peek = function (o) { return this.t[this.p + (o || 0)] || this.t[this.t.length - 1]; };
  Parser.prototype.at = function (k, v) { var t = this.peek(); return t.k === k && (v === undefined || t.v === v); };
  Parser.prototype.atOp = function (v) { return this.at('OP', v); };
  Parser.prototype.atName = function (v) { return this.at('NAME', v); };
  Parser.prototype.next = function () { return this.t[this.p++]; };
  Parser.prototype.line = function () { return this.peek().line; };
  Parser.prototype.eat = function (k, v) { if (this.at(k, v)) { return this.next(); } return null; };
  Parser.prototype.expect = function (k, v, what) {
    if (this.at(k, v)) return this.next();
    var t = this.peek();
    var got = t.k === 'EOF' ? '파일 끝' : (t.k === 'NEWLINE' ? '줄바꿈' : "'" + t.v + "'");
    throw ce(t.line, this.lang === 'cpp' ? 'C2143' : 'SyntaxError',
      (what || "'" + v + "'") + " 이(가) 필요한데 " + got + " 이(가) 왔습니다.");
  };

  /* ---- 식 ---- */
  Parser.prototype.parseExpr = function () { return this.parseAssignExpr(); };

  Parser.prototype.parseAssignExpr = function () {
    var left = this.parseOr();
    var ops = ['=', '+=', '-=', '*=', '/=', '%='];
    for (var i = 0; i < ops.length; i++) {
      if (this.atOp(ops[i])) {
        var line = this.line(); var op = this.next().v;
        var right = this.parseAssignExpr();
        if (left.type !== 'Name' && left.type !== 'Index') {
          throw ce(line, this.lang === 'cpp' ? 'C2106' : 'SyntaxError', '대입은 변수 이름에만 할 수 있습니다.');
        }
        return { type: 'Assign', op: op, target: left, value: right, line: line };
      }
    }
    return left;
  };

  Parser.prototype.parseOr = function () {
    var left = this.parseAnd();
    while ((this.lang === 'cpp' && this.atOp('||')) || (this.lang === 'py' && this.atName('or'))) {
      var line = this.line(); this.next();
      left = { type: 'Logic', op: 'or', left: left, right: this.parseAnd(), line: line };
    }
    return left;
  };
  Parser.prototype.parseAnd = function () {
    var left = this.parseNot();
    while ((this.lang === 'cpp' && this.atOp('&&')) || (this.lang === 'py' && this.atName('and'))) {
      var line = this.line(); this.next();
      left = { type: 'Logic', op: 'and', left: left, right: this.parseNot(), line: line };
    }
    return left;
  };
  Parser.prototype.parseNot = function () {
    if ((this.lang === 'cpp' && this.atOp('!')) || (this.lang === 'py' && this.atName('not'))) {
      var line = this.line(); this.next();
      return { type: 'Unary', op: '!', arg: this.parseNot(), line: line };
    }
    return this.parseCompare();
  };
  Parser.prototype.parseCompare = function () {
    var left = this.parseAdd();
    while (this.atOp('==') || this.atOp('!=') || this.atOp('<') || this.atOp('>') || this.atOp('<=') || this.atOp('>=')) {
      var line = this.line(); var op = this.next().v;
      left = { type: 'Bin', op: op, left: left, right: this.parseAdd(), line: line };
    }
    return left;
  };
  Parser.prototype.parseAdd = function () {
    var left = this.parseMul();
    while (this.atOp('+') || this.atOp('-')) {
      var line = this.line(); var op = this.next().v;
      left = { type: 'Bin', op: op, left: left, right: this.parseMul(), line: line };
    }
    return left;
  };
  Parser.prototype.parseMul = function () {
    var left = this.parseUnary();
    while (this.atOp('*') || this.atOp('/') || this.atOp('%') || (this.lang === 'py' && this.atOp('//'))) {
      var line = this.line(); var op = this.next().v;
      left = { type: 'Bin', op: op, left: left, right: this.parseUnary(), line: line };
    }
    return left;
  };
  Parser.prototype.parseUnary = function () {
    if (this.atOp('-') || this.atOp('+')) {
      var line = this.line(); var op = this.next().v;
      return { type: 'Unary', op: op, arg: this.parseUnary(), line: line };
    }
    if (this.lang === 'cpp' && (this.atOp('++') || this.atOp('--'))) {
      var l2 = this.line(); var op2 = this.next().v;
      return { type: 'IncDec', op: op2, prefix: true, target: this.parseUnary(), line: l2 };
    }
    return this.parsePostfix();
  };
  Parser.prototype.parsePostfix = function () {
    var e = this.parsePrimary();
    for (;;) {
      if (this.atOp('(')) {
        var line = this.line(); this.next();
        var args = [];
        if (!this.atOp(')')) {
          do { args.push(this.parseExpr()); } while (this.eat('OP', ','));
        }
        this.expect('OP', ')', "')'");
        e = { type: 'Call', callee: e, args: args, line: line };
      } else if (this.atOp('[')) {
        var l3 = this.line(); this.next();
        var idx = this.parseExpr();
        this.expect('OP', ']', "']'");
        e = { type: 'Index', target: e, index: idx, line: l3 };
      } else if (this.lang === 'cpp' && (this.atOp('++') || this.atOp('--'))) {
        var l4 = this.line(); var op4 = this.next().v;
        e = { type: 'IncDec', op: op4, prefix: false, target: e, line: l4 };
      } else break;
    }
    return e;
  };
  Parser.prototype.parsePrimary = function () {
    var t = this.peek();
    if (t.k === 'NUM') { this.next(); return { type: 'Lit', kind: t.v.f ? 'real' : 'int', value: t.v.v, line: t.line }; }
    if (t.k === 'STR') { this.next(); return { type: 'Lit', kind: 'str', value: t.v, line: t.line }; }
    if (this.atOp('(')) {
      // (double)a 같은 C 스타일 캐스트
      if (this.lang === 'cpp' && this.peek(1).k === 'NAME' && CPP_TYPES.indexOf(this.peek(1).v) >= 0
          && this.peek(2).k === 'OP' && this.peek(2).v === ')') {
        var cl = this.line();
        this.next();
        var ct = this.parseCppType();
        this.expect('OP', ')', "')'");
        return { type: 'Cast', vtype: ct, arg: this.parseUnary(), line: cl };
      }
      this.next();
      var e = this.parseExpr();
      this.expect('OP', ')', "')'");
      return e;
    }
    if (this.lang === 'cpp' && this.atName('static_cast')) {
      var sl = this.line();
      this.next();
      this.expect('OP', '<', "'<'");
      var st = this.parseCppType();
      this.expect('OP', '>', "'>'");
      this.expect('OP', '(', "'('");
      var sa = this.parseExpr();
      this.expect('OP', ')', "')'");
      return { type: 'Cast', vtype: st, arg: sa, line: sl };
    }
    if (this.lang === 'py' && this.atOp('[')) {
      var line = this.line(); this.next();
      var items = [];
      if (!this.atOp(']')) { do { items.push(this.parseExpr()); } while (this.eat('OP', ',')); }
      this.expect('OP', ']', "']'");
      return { type: 'ListLit', items: items, line: line };
    }
    if (t.k === 'NAME') {
      this.next();
      if (this.lang === 'cpp') {
        if (t.v === 'true' || t.v === 'false') return { type: 'Lit', kind: 'bool', value: t.v === 'true', line: t.line };
        if (t.v === 'nullptr') return { type: 'Lit', kind: 'none', value: null, line: t.line };
      } else {
        if (t.v === 'True' || t.v === 'False') return { type: 'Lit', kind: 'bool', value: t.v === 'True', line: t.line };
        if (t.v === 'None') return { type: 'Lit', kind: 'none', value: null, line: t.line };
      }
      return { type: 'Name', name: t.v, line: t.line };
    }
    throw ce(t.line, this.lang === 'cpp' ? 'C2059' : 'SyntaxError',
      '여기서는 값이 와야 합니다. (' + (t.k === 'EOF' ? '파일 끝' : "'" + t.v + "'") + ')');
  };

  /* ---- C++ 문 ---- */
  Parser.prototype.isTypeStart = function () {
    if (this.atName('const')) return true;
    var t = this.peek();
    return t.k === 'NAME' && CPP_TYPES.indexOf(t.v) >= 0;
  };
  Parser.prototype.parseCppType = function () {
    if (this.atName('const')) this.next();
    var t = this.expect('NAME', undefined, '자료형');
    var name = t.v;
    while (this.atOp('*') || this.atOp('&')) { name += this.next().v; }
    if (name === 'std::string') name = 'string';
    if (name === 'float') name = 'double';
    if (name === 'long' || name === 'short') name = 'int';
    return name;
  };

  Parser.prototype.parseCppProgram = function () {
    var body = [];
    while (!this.at('EOF')) body.push(this.parseCppTopLevel());
    return { type: 'Program', body: body };
  };

  Parser.prototype.parseCppTopLevel = function () {
    // 함수 정의: <type> <name> ( ... ) { ... }
    if (this.isTypeStart()) {
      var save = this.p;
      var vtype = this.parseCppType();
      if (this.at('NAME') && this.peek(1).k === 'OP' && this.peek(1).v === '(') {
        var nameTok = this.next();
        var line = nameTok.line;
        this.expect('OP', '(', "'('");
        var params = [];
        if (!this.atOp(')')) {
          do {
            var pt = this.parseCppType();
            var pn = this.expect('NAME', undefined, '매개변수 이름');
            params.push({ vtype: pt, name: pn.v, line: pn.line });
          } while (this.eat('OP', ','));
        }
        this.expect('OP', ')', "')'");
        if (this.atOp(';')) { this.next(); return { type: 'Nop', line: line }; }   // 선언만
        var block = this.parseCppBlock();
        return { type: 'FuncDecl', name: nameTok.v, vtype: vtype, params: params, body: block, line: line };
      }
      this.p = save;
    }
    return this.parseCppStmt();
  };

  Parser.prototype.parseCppBlock = function () {
    var open = this.expect('OP', '{', "'{'");
    var body = [];
    while (!this.atOp('}')) {
      if (this.at('EOF')) throw ce(open.line, 'C1075', "'{' 에 맞는 '}' 를 찾지 못했습니다.");
      body.push(this.parseCppStmt());
    }
    var close = this.next();
    return { type: 'Block', body: body, line: open.line, endLine: close.line };
  };

  Parser.prototype.parseCppStmt = function () {
    var t = this.peek();
    if (this.atOp('{')) return this.parseCppBlock();
    if (this.atOp(';')) { this.next(); return { type: 'Nop', line: t.line }; }
    if (t.k === 'NAME') {
      switch (t.v) {
        case 'using': while (!this.atOp(';') && !this.at('EOF')) this.next(); this.eat('OP', ';'); return { type: 'Nop', line: t.line };
        case 'if': return this.parseCppIf();
        case 'while': return this.parseCppWhile();
        case 'for': return this.parseCppFor();
        case 'return': {
          this.next();
          var val = this.atOp(';') ? null : this.parseExpr();
          this.expect('OP', ';', "';'");
          return { type: 'Return', value: val, line: t.line };
        }
        case 'break': this.next(); this.expect('OP', ';', "';'"); return { type: 'Break', line: t.line };
        case 'continue': this.next(); this.expect('OP', ';', "';'"); return { type: 'Continue', line: t.line };
        case 'cout': case 'std::cout': return this.parseCout();
      }
    }
    if (this.isTypeStart()) return this.parseCppVarDecl();
    var e = this.parseExpr();
    this.expect('OP', ';', "';'");
    return { type: 'ExprStmt', expr: e, line: t.line };
  };

  Parser.prototype.parseCppVarDecl = function () {
    var line = this.line();
    var vtype = this.parseCppType();
    var decls = [];
    do {
      var nameTok = this.expect('NAME', undefined, '변수 이름');
      if (this.atOp('[')) throw ce(nameTok.line, 'CSV001', '배열 선언은 아직 지원하지 않습니다.');
      var init = null;
      if (this.eat('OP', '=')) init = this.parseExpr();
      else if (this.atOp('(')) {                       // int x(3);
        this.next(); init = this.parseExpr(); this.expect('OP', ')', "')'");
      }
      decls.push({ name: nameTok.v, init: init, line: nameTok.line });
    } while (this.eat('OP', ','));
    this.expect('OP', ';', "';'");
    return { type: 'VarDecl', vtype: vtype, decls: decls, line: line };
  };

  Parser.prototype.parseCout = function () {
    var line = this.line();
    this.next();
    var parts = [];
    while (this.atOp('<<')) {
      this.next();
      var e = this.parseExpr();
      if (e.type === 'Name' && (e.name === 'endl' || e.name === 'std::endl')) parts.push({ type: 'Endl', line: e.line });
      else parts.push(e);
    }
    this.expect('OP', ';', "';'");
    return { type: 'Out', parts: parts, line: line };
  };

  Parser.prototype.parseCppIf = function () {
    var line = this.line();
    this.next();
    this.expect('OP', '(', "'('");
    var cond = this.parseExpr();
    this.expect('OP', ')', "')'");
    var then = this.parseCppStmt();
    var alt = null;
    if (this.atName('else')) { this.next(); alt = this.parseCppStmt(); }
    return { type: 'If', cond: cond, then: then, alt: alt, line: line };
  };
  Parser.prototype.parseCppWhile = function () {
    var line = this.line();
    this.next();
    this.expect('OP', '(', "'('");
    var cond = this.parseExpr();
    this.expect('OP', ')', "')'");
    return { type: 'While', cond: cond, body: this.parseCppStmt(), line: line };
  };
  Parser.prototype.parseCppFor = function () {
    var line = this.line();
    this.next();
    this.expect('OP', '(', "'('");
    var init = null;
    if (this.atOp(';')) this.next();
    else if (this.isTypeStart()) init = this.parseCppVarDecl();
    else { init = { type: 'ExprStmt', expr: this.parseExpr(), line: line }; this.expect('OP', ';', "';'"); }
    var cond = this.atOp(';') ? null : this.parseExpr();
    this.expect('OP', ';', "';'");
    var step = this.atOp(')') ? null : this.parseExpr();
    this.expect('OP', ')', "')'");
    return { type: 'For', init: init, cond: cond, step: step, body: this.parseCppStmt(), line: line };
  };

  /* ---- Python 문 ---- */
  Parser.prototype.parsePyProgram = function () {
    var body = [];
    while (!this.at('EOF')) {
      if (this.at('NEWLINE') || this.at('INDENT') || this.at('DEDENT')) { this.next(); continue; }
      body.push(this.parsePyStmt());
    }
    return { type: 'Program', body: body };
  };

  Parser.prototype.parsePyBlock = function () {
    this.expect('OP', ':', "':'");
    if (!this.at('NEWLINE')) {                     // 한 줄 블록: if x: y = 1
      var s = this.parsePySimple();
      return { type: 'Block', body: [s], line: s.line, endLine: s.line };
    }
    this.expect('NEWLINE');
    var ind = this.expect('INDENT', undefined, '들여쓰기');
    var body = [], last = ind.line;
    while (!this.at('DEDENT') && !this.at('EOF')) {
      if (this.at('NEWLINE')) { this.next(); continue; }
      var st = this.parsePyStmt();
      body.push(st); last = st.endLine || st.line;
    }
    var d = this.eat('DEDENT');
    return { type: 'Block', body: body, line: ind.line, endLine: d ? d.line : last };
  };

  Parser.prototype.parsePyStmt = function () {
    var t = this.peek();
    if (t.k === 'NAME') {
      switch (t.v) {
        case 'if': return this.parsePyIf();
        case 'while': {
          this.next();
          var cond = this.parseExpr();
          var body = this.parsePyBlock();
          return { type: 'While', cond: cond, body: body, line: t.line, endLine: body.endLine };
        }
        case 'for': {
          this.next();
          var nameTok = this.expect('NAME', undefined, '반복 변수 이름');
          if (!this.atName('in')) throw ce(this.line(), 'SyntaxError', "'in' 이 필요합니다.");
          this.next();
          var iter = this.parseExpr();
          var fbody = this.parsePyBlock();
          return { type: 'ForIn', name: nameTok.v, iter: iter, body: fbody, line: t.line, endLine: fbody.endLine };
        }
        case 'def': {
          this.next();
          var fname = this.expect('NAME', undefined, '함수 이름');
          this.expect('OP', '(', "'('");
          var params = [];
          if (!this.atOp(')')) {
            do {
              var pn = this.expect('NAME', undefined, '매개변수 이름');
              params.push({ name: pn.v, line: pn.line });
            } while (this.eat('OP', ','));
          }
          this.expect('OP', ')', "')'");
          var dbody = this.parsePyBlock();
          return { type: 'FuncDecl', name: fname.v, params: params, body: dbody, line: t.line, endLine: dbody.endLine };
        }
      }
    }
    return this.parsePySimple();
  };

  Parser.prototype.parsePySimple = function () {
    var t = this.peek();
    var node;
    if (t.k === 'NAME' && t.v === 'return') {
      this.next();
      var val = this.at('NEWLINE') || this.at('EOF') ? null : this.parseExpr();
      node = { type: 'Return', value: val, line: t.line };
    } else if (t.k === 'NAME' && t.v === 'pass') {
      this.next(); node = { type: 'Nop', line: t.line };
    } else if (t.k === 'NAME' && t.v === 'break') {
      this.next(); node = { type: 'Break', line: t.line };
    } else if (t.k === 'NAME' && t.v === 'continue') {
      this.next(); node = { type: 'Continue', line: t.line };
    } else if (t.k === 'NAME' && t.v === 'global') {
      this.next();
      var names = [];
      do { names.push(this.expect('NAME', undefined, '이름').v); } while (this.eat('OP', ','));
      node = { type: 'Global', names: names, line: t.line };
    } else {
      var e = this.parseExpr();
      node = e.type === 'Assign'
        ? { type: 'Assign', op: e.op, target: e.target, value: e.value, line: t.line }
        : { type: 'ExprStmt', expr: e, line: t.line };
    }
    if (!this.at('EOF')) this.expect('NEWLINE', undefined, '줄바꿈');
    return node;
  };

  Parser.prototype.parsePyIf = function () {
    var t = this.next();                       // if / elif
    var cond = this.parseExpr();
    var then = this.parsePyBlock();
    var alt = null;
    if (this.atName('elif')) {
      alt = this.parsePyIf();
    } else if (this.atName('else')) {
      this.next();
      alt = this.parsePyBlock();
    }
    return { type: 'If', cond: cond, then: then, alt: alt, line: t.line };
  };

  function parse(src, lang) {
    var p = new Parser(lang === 'cpp' ? lexCpp(src) : lexPy(src), lang);
    return lang === 'cpp' ? p.parseCppProgram() : p.parsePyProgram();
  }

  /* =============================================================
     3. C++ 정적 검사 — "위에서 아래로, 그 순간 이름이 있어야 한다"
     ============================================================= */
  var CPP_BUILTIN = ['endl', 'std::endl', 'cout', 'std::cout', 'true', 'false'];

  function checkCpp(ast) {
    var errors = [];
    var scopes = [Object.create(null)];                 // 전역부터
    function declare(name, line, kind) { scopes[scopes.length - 1][name] = { line: line, kind: kind }; }
    function visible(name) {
      for (var i = scopes.length - 1; i >= 0; i--) if (scopes[i][name]) return scopes[i][name];
      return null;
    }
    function use(node) {
      if (!node) return;
      switch (node.type) {
        case 'Name':
          if (CPP_BUILTIN.indexOf(node.name) >= 0) return;
          if (!visible(node.name)) {
            errors.push({ line: node.line, code: 'C2065', msg: "'" + node.name + "': 선언되지 않은 식별자입니다." });
          }
          return;
        case 'Bin': case 'Logic': use(node.left); use(node.right); return;
        case 'Unary': use(node.arg); return;
        case 'Cast': use(node.arg); return;
        case 'IncDec': use(node.target); return;
        case 'Index': use(node.target); use(node.index); return;
        case 'Assign':
          use(node.target); use(node.value); return;
        case 'Call':
          if (node.callee.type === 'Name') {
            var f = visible(node.callee.name);
            if (!f) errors.push({ line: node.line, code: 'C3861', msg: "'" + node.callee.name + "': 식별자를 찾을 수 없습니다." });
            else if (f.kind !== 'func') errors.push({ line: node.line, code: 'C2064', msg: "'" + node.callee.name + "': 함수가 아닙니다." });
          } else use(node.callee);
          for (var i = 0; i < node.args.length; i++) use(node.args[i]);
          return;
        case 'Lit': case 'Endl': return;
        default: return;
      }
    }
    function walk(s) {
      if (!s) return;
      switch (s.type) {
        case 'Program': for (var i = 0; i < s.body.length; i++) walk(s.body[i]); return;
        case 'Block':
          scopes.push(Object.create(null));
          for (var j = 0; j < s.body.length; j++) walk(s.body[j]);
          scopes.pop();
          return;
        case 'VarDecl':
          for (var k = 0; k < s.decls.length; k++) {
            var d = s.decls[k];
            use(d.init);                                     // 초기화식이 먼저 검사된다
            if (scopes[scopes.length - 1][d.name]) {
              errors.push({ line: d.line, code: 'C2374', msg: "'" + d.name + "': 재정의입니다. 같은 블록에서 이미 선언되었습니다." });
            }
            declare(d.name, d.line, 'var');
          }
          return;
        case 'FuncDecl':
          declare(s.name, s.line, 'func');                   // 이 지점부터 호출 가능
          scopes.push(Object.create(null));
          for (var q = 0; q < s.params.length; q++) declare(s.params[q].name, s.params[q].line, 'var');
          for (var r = 0; r < s.body.body.length; r++) walk(s.body.body[r]);
          scopes.pop();
          return;
        case 'ExprStmt': use(s.expr); return;
        case 'Out': for (var m = 0; m < s.parts.length; m++) if (s.parts[m].type !== 'Endl') use(s.parts[m]); return;
        case 'If': use(s.cond); walk(s.then); walk(s.alt); return;
        case 'While': use(s.cond); walk(s.body); return;
        case 'For':
          scopes.push(Object.create(null));
          walk(s.init); use(s.cond); use(s.step); walk(s.body);
          scopes.pop();
          return;
        case 'Return': use(s.value); return;
        default: return;
      }
    }
    walk(ast);
    errors.sort(function (a, b) { return a.line - b.line; });
    return errors;
  }

  /* =============================================================
     4. 값 / 서식
     ============================================================= */
  function V(t, v, extra) {
    var o = { t: t, v: v };
    if (extra) for (var k in extra) o[k] = extra[k];
    return o;
  }
  var GARBAGE = {
    int: -858993460,                 // MSVC 디버그 빌드 0xCCCCCCCC
    double: -9.2559631349317831e+61,
    bool: true,
    char: 'Ì'
  };

  function fmtNum(v) {
    if (Number.isInteger(v) && Math.abs(v) < 1e15) return String(v);
    if (Math.abs(v) >= 1e15 || (v !== 0 && Math.abs(v) < 1e-4)) return v.toExponential(6).replace('e', 'e');
    return String(parseFloat(v.toPrecision(10)));
  }
  function fmtCpp(val) {
    if (!val) return '';
    switch (val.t) {
      case 'int': return String(val.v);
      case 'double': return Number.isInteger(val.v) && Math.abs(val.v) < 1e15 ? val.v.toFixed(5) : fmtNum(val.v);
      case 'bool': return val.v ? 'true' : 'false';
      case 'string': return '"' + String(val.v) + '"';
      case 'void': return '';
      default: return String(val.v);
    }
  }
  function fmtPy(val) {
    if (!val) return '';
    switch (val.t) {
      case 'int': return String(val.v);
      case 'float': return Number.isInteger(val.v) ? val.v.toFixed(1) : fmtNum(val.v);
      case 'bool': return val.v ? 'True' : 'False';
      case 'str': return "'" + String(val.v) + "'";
      case 'none': return 'None';
      case 'func': return '<function ' + val.v.name + '>';
      case 'list': {
        var out = [];
        for (var i = 0; i < val.v.length; i++) out.push(fmtPy(val.v[i]));
        return '[' + out.join(', ') + ']';
      }
      default: return String(val.v);
    }
  }
  function pyTypeName(val) {
    if (!val) return '?';
    if (val.t === 'str') return 'str';
    if (val.t === 'none') return 'NoneType';
    if (val.t === 'func') return 'function';
    return val.t;
  }
  function toStrOut(val, lang) {
    if (!val) return '';
    if (lang === 'cpp') {
      if (val.t === 'bool') return val.v ? '1' : '0';
      if (val.t === 'double') return Number.isInteger(val.v) ? String(val.v) : fmtNum(val.v);
      return String(val.v);
    }
    if (val.t === 'bool') return val.v ? 'True' : 'False';
    if (val.t === 'none') return 'None';
    if (val.t === 'float') return Number.isInteger(val.v) ? val.v.toFixed(1) : fmtNum(val.v);
    if (val.t === 'list' || val.t === 'func') return fmtPy(val);
    return String(val.v);
  }

  function truthy(val, lang) {
    if (!val) return false;
    if (lang === 'cpp') {
      if (val.t === 'string') return val.v.length > 0;
      return !!val.v;
    }
    if (val.t === 'none') return false;
    if (val.t === 'str') return val.v.length > 0;
    if (val.t === 'list') return val.v.length > 0;
    return !!val.v;
  }

  /* =============================================================
     5. 실행기
     ============================================================= */
  function makeCtx(lang) {
    return {
      lang: lang,
      out: [],
      frames: [],
      globals: null,
      funcs: Object.create(null),
      steps: 0,
      lastNote: null
    };
  }

  /* ---------- C++ 런타임 ---------- */
  function cppDefault(vtype) {
    if (vtype === 'string') return V('string', '');            // 클래스라 항상 기본 생성
    return null;                                               // 나머지는 미초기화
  }
  function cppCoerce(vtype, val, line) {
    if (!val) return null;
    if (vtype === 'auto') return val;
    if (vtype === 'int') {
      if (val.t === 'string') throw rt(line, 'C2440', "string 값을 int 변수에 넣을 수 없습니다.");
      return V('int', Math.trunc(val.t === 'bool' ? (val.v ? 1 : 0) : val.v));
    }
    if (vtype === 'double') {
      if (val.t === 'string') throw rt(line, 'C2440', "string 값을 double 변수에 넣을 수 없습니다.");
      return V('double', val.t === 'bool' ? (val.v ? 1 : 0) : val.v);
    }
    if (vtype === 'bool') return V('bool', truthy(val, 'cpp'));
    if (vtype === 'string') {
      if (val.t !== 'string') throw rt(line, 'C2440', "숫자를 string 변수에 바로 넣을 수 없습니다.");
      return V('string', val.v);
    }
    return val;
  }
  function numOf(val) { return val.t === 'bool' ? (val.v ? 1 : 0) : val.v; }

  function binCpp(op, a, b, line) {
    if (op === '+' && (a.t === 'string' || b.t === 'string')) {
      if (a.t !== 'string' || b.t !== 'string') {
        throw rt(line, 'C2110', 'string 과 숫자는 + 로 이을 수 없습니다. (std::to_string 필요)');
      }
      return V('string', a.v + b.v);
    }
    if (op === '==' || op === '!=' || op === '<' || op === '>' || op === '<=' || op === '>=') {
      var x = a.t === 'string' ? a.v : numOf(a), y = b.t === 'string' ? b.v : numOf(b);
      var r;
      switch (op) {
        case '==': r = x === y; break;
        case '!=': r = x !== y; break;
        case '<': r = x < y; break;
        case '>': r = x > y; break;
        case '<=': r = x <= y; break;
        default: r = x >= y;
      }
      return V('bool', r);
    }
    if (a.t === 'string' || b.t === 'string') throw rt(line, 'C2110', 'string 에는 이 연산자를 쓸 수 없습니다.');
    var isD = a.t === 'double' || b.t === 'double';
    var l = numOf(a), rr = numOf(b), res;
    switch (op) {
      case '+': res = l + rr; break;
      case '-': res = l - rr; break;
      case '*': res = l * rr; break;
      case '/':
        if (rr === 0 && !isD) throw rt(line, '', '정수를 0으로 나눌 수 없습니다.');
        res = l / rr; break;
      case '%':
        if (isD) throw rt(line, 'C2296', '% 는 정수끼리만 쓸 수 있습니다.');
        if (rr === 0) throw rt(line, '', '0으로 나눈 나머지는 정의되지 않습니다.');
        res = l % rr; break;
      default: throw rt(line, '', "연산자 '" + op + "' 는 지원하지 않습니다.");
    }
    return isD ? V('double', res) : V('int', Math.trunc(res));
  }

  function binPy(op, a, b, line) {
    if (op === '+' && (a.t === 'str' || b.t === 'str')) {
      if (a.t !== 'str' || b.t !== 'str') {
        throw rt(line, 'TypeError',
          a.t === 'str'
            ? 'can only concatenate str (not "' + pyTypeName(b) + '") to str'
            : "unsupported operand type(s) for +: '" + pyTypeName(a) + "' and 'str'");
      }
      return V('str', a.v + b.v);
    }
    if (op === '*' && (a.t === 'str' || b.t === 'str')) {
      var s = a.t === 'str' ? a : b, k = a.t === 'str' ? b : a;
      if (k.t !== 'int' && k.t !== 'bool') throw rt(line, 'TypeError', "can't multiply sequence by non-int");
      return V('str', s.v.repeat(Math.max(0, numOf(k))));
    }
    if (op === '==' || op === '!=') {
      var eq = a.t === b.t ? a.v === b.v : numOf2(a) === numOf2(b);
      if (a.t === 'str' || b.t === 'str') eq = a.t === b.t && a.v === b.v;
      return V('bool', op === '==' ? eq : !eq);
    }
    if (op === '<' || op === '>' || op === '<=' || op === '>=') {
      if ((a.t === 'str') !== (b.t === 'str')) {
        throw rt(line, 'TypeError', "'" + op + "' not supported between instances of '" + pyTypeName(a) + "' and '" + pyTypeName(b) + "'");
      }
      var x = a.t === 'str' ? a.v : numOf2(a), y = b.t === 'str' ? b.v : numOf2(b);
      var r2;
      switch (op) {
        case '<': r2 = x < y; break;
        case '>': r2 = x > y; break;
        case '<=': r2 = x <= y; break;
        default: r2 = x >= y;
      }
      return V('bool', r2);
    }
    if (a.t === 'str' || b.t === 'str' || a.t === 'none' || b.t === 'none') {
      throw rt(line, 'TypeError', "unsupported operand type(s) for " + op + ": '" + pyTypeName(a) + "' and '" + pyTypeName(b) + "'");
    }
    var isF = a.t === 'float' || b.t === 'float';
    var l2 = numOf2(a), r3 = numOf2(b), res2;
    switch (op) {
      case '+': res2 = l2 + r3; break;
      case '-': res2 = l2 - r3; break;
      case '*': res2 = l2 * r3; break;
      case '/':
        if (r3 === 0) throw rt(line, 'ZeroDivisionError', 'division by zero');
        return V('float', l2 / r3);                              // 파이썬 / 는 항상 float
      case '//':
        if (r3 === 0) throw rt(line, 'ZeroDivisionError', 'integer division or modulo by zero');
        res2 = Math.floor(l2 / r3); break;
      case '%':
        if (r3 === 0) throw rt(line, 'ZeroDivisionError', 'integer division or modulo by zero');
        res2 = ((l2 % r3) + r3) % r3; break;
      case '**': res2 = Math.pow(l2, r3); break;
      default: throw rt(line, 'SyntaxError', "연산자 '" + op + "' 는 지원하지 않습니다.");
    }
    return isF ? V('float', res2) : V('int', res2);
  }
  function numOf2(val) { return val.t === 'bool' ? (val.v ? 1 : 0) : val.v; }

  /* ---------- 스코프 ---------- */
  function newFrame(name, lang, line) {
    return { name: name, scopes: [Object.create(null)], line: line, lang: lang, localNames: null, globalNames: null };
  }
  function declareVar(frame, name, cell) { frame.scopes[frame.scopes.length - 1][name] = cell; }
  function findCell(ctx, name) {
    var f = ctx.frames[ctx.frames.length - 1];
    for (var i = f.scopes.length - 1; i >= 0; i--) if (Object.prototype.hasOwnProperty.call(f.scopes[i], name)) return f.scopes[i][name];
    if (ctx.lang === 'cpp' && ctx.globals && Object.prototype.hasOwnProperty.call(ctx.globals, name)) return ctx.globals[name];
    return null;
  }

  /* ---------- 평가 (제너레이터) ---------- */
  function pause(line, note, opts) {
    var o = { line: line, note: note };
    if (opts) for (var k in opts) o[k] = opts[k];
    return o;
  }

  function* evalExpr(node, ctx) {
    switch (node.type) {
      case 'Lit': {
        if (node.kind === 'int') return V(ctx.lang === 'cpp' ? 'int' : 'int', node.value);
        if (node.kind === 'real') return V(ctx.lang === 'cpp' ? 'double' : 'float', node.value);
        if (node.kind === 'str') return V(ctx.lang === 'cpp' ? 'string' : 'str', node.value);
        if (node.kind === 'bool') return V('bool', node.value);
        return V('none', null);
      }
      case 'ListLit': {
        var items = [];
        for (var i = 0; i < node.items.length; i++) items.push(yield* evalExpr(node.items[i], ctx));
        return V('list', items);
      }
      case 'Name': return yield* readName(node, ctx);
      case 'Logic': {
        var a = yield* evalExpr(node.left, ctx);
        var at = truthy(a, ctx.lang);
        if (node.op === 'and' && !at) return ctx.lang === 'cpp' ? V('bool', false) : a;
        if (node.op === 'or' && at) return ctx.lang === 'cpp' ? V('bool', true) : a;
        var b = yield* evalExpr(node.right, ctx);
        if (ctx.lang === 'cpp') return V('bool', truthy(b, 'cpp'));
        return b;
      }
      case 'Unary': {
        var v = yield* evalExpr(node.arg, ctx);
        if (node.op === '!') return V('bool', !truthy(v, ctx.lang));
        if (node.op === '-') {
          if (v.t === 'str' || v.t === 'string') throw rt(node.line, ctx.lang === 'cpp' ? 'C2675' : 'TypeError', '문자열에는 - 를 쓸 수 없습니다.');
          return V(v.t === 'bool' ? 'int' : v.t, -numOf(v));
        }
        return v;
      }
      case 'Bin': {
        var l = yield* evalExpr(node.left, ctx);
        var r = yield* evalExpr(node.right, ctx);
        if (ctx.lang === 'cpp') {
          var res = binCpp(node.op, l, r, node.line);
          if (node.op === '/' && l.t === 'int' && r.t === 'int' && numOf(l) % numOf(r) !== 0) {
            ctx.pendingInsight = { kind: 'intdiv', text: 'int ÷ int 는 소수점을 버립니다. ' + numOf(l) + ' / ' + numOf(r) + ' → ' + res.v + ' (실수 결과가 필요하면 한쪽을 double 로)' };
          }
          return res;
        }
        return binPy(node.op, l, r, node.line);
      }
      case 'Index': {
        var target = yield* evalExpr(node.target, ctx);
        var idx = yield* evalExpr(node.index, ctx);
        if (target.t !== 'list' && target.t !== 'str' && target.t !== 'string') {
          throw rt(node.line, 'TypeError', "'" + pyTypeName(target) + "' object is not subscriptable");
        }
        var arr = target.t === 'list' ? target.v : String(target.v).split('');
        var i2 = numOf2(idx);
        if (i2 < 0) i2 += arr.length;
        if (i2 < 0 || i2 >= arr.length) throw rt(node.line, 'IndexError', 'list index out of range');
        return target.t === 'list' ? arr[i2] : V('str', arr[i2]);
      }
      case 'Cast': {
        var cv = yield* evalExpr(node.arg, ctx);
        return cppCoerce(node.vtype, cv, node.line);
      }
      case 'Assign': return yield* doAssign(node, ctx);
      case 'IncDec': {
        var cell = yield* resolveCell(node.target, ctx);
        var before = cell.value;
        if (!before) { before = yield* readName(node.target, ctx); }
        var delta = node.op === '++' ? 1 : -1;
        var after = V(before.t, numOf(before) + delta);
        cell.value = after; cell.init = true;
        return node.prefix ? after : before;
      }
      case 'Call': return yield* doCall(node, ctx);
      default:
        throw rt(node.line, '', "지원하지 않는 식입니다. (" + node.type + ")");
    }
  }

  function* readName(node, ctx) {
    var name = node.name;
    if (ctx.lang === 'cpp') {
      var cell = findCell(ctx, name);
      if (!cell) {
        if (ctx.funcs[name]) return V('func', ctx.funcs[name]);
        throw rt(node.line, 'C2065', "'" + name + "': 선언되지 않은 식별자입니다.");
      }
      if (!cell.init) {
        ctx.pendingInsight = {
          kind: 'uninit',
          text: "warning C4700: 초기화되지 않은 '" + name + "' 지역 변수를 사용했습니다. "
            + "이 줄에서 읽히는 값은 스택에 남아 있던 쓰레기라, 실행할 때마다 달라질 수 있어요."
        };
        cell.readUninit = true;
      }
      return cell.value || V(cell.vtype === 'double' ? 'double' : cell.vtype, garbageFor(cell.vtype));
    }
    // Python
    var f = ctx.frames[ctx.frames.length - 1];
    if (!f.isGlobal) {
      if (Object.prototype.hasOwnProperty.call(f.scopes[0], name) && f.scopes[0][name].init) return f.scopes[0][name].value;
      if (f.localNames && f.localNames[name] && !(f.globalNames && f.globalNames[name])) {
        throw rt(node.line, 'UnboundLocalError',
          "cannot access local variable '" + name + "' where it is not associated with a value "
          + "— 이 함수 안에서 나중에 " + name + " 에 대입하니까 파이썬은 " + name + " 을 지역 이름으로 봅니다.");
      }
    }
    if (Object.prototype.hasOwnProperty.call(ctx.globals, name) && ctx.globals[name].init) return ctx.globals[name].value;
    if (PY_BUILTINS[name]) return V('builtin', name);
    throw rt(node.line, 'NameError', "name '" + name + "' is not defined "
      + "— 아직 " + name + " 에 대입한 적이 없어서 이름 자체가 없습니다.");
  }

  function garbageFor(vtype) {
    if (vtype === 'int') return GARBAGE.int;
    if (vtype === 'double') return GARBAGE.double;
    if (vtype === 'bool') return GARBAGE.bool;
    if (vtype === 'char') return GARBAGE.char;
    return 0;
  }

  function* resolveCell(target, ctx) {
    if (target.type !== 'Name') throw rt(target.line, '', '이 대상에는 대입할 수 없습니다.');
    var name = target.name;
    if (ctx.lang === 'cpp') {
      var cell = findCell(ctx, name);
      if (!cell) throw rt(target.line, 'C2065', "'" + name + "': 선언되지 않은 식별자입니다.");
      return cell;
    }
    var f = ctx.frames[ctx.frames.length - 1];
    var scope = (f.isGlobal || (f.globalNames && f.globalNames[name])) ? ctx.globals : f.scopes[0];
    if (!Object.prototype.hasOwnProperty.call(scope, name)) {
      scope[name] = { name: name, value: null, init: false, vtype: null, fresh: true };
    }
    return scope[name];
  }

  function* doAssign(node, ctx) {
    var val;
    if (node.op === '=') {
      val = yield* evalExpr(node.value, ctx);
    } else {
      var cur = yield* readName(node.target, ctx);           // 없는 이름이면 여기서 바로 오류
      var rhs = yield* evalExpr(node.value, ctx);
      var op = node.op[0];
      val = ctx.lang === 'cpp' ? binCpp(op, cur, rhs, node.line) : binPy(op, cur, rhs, node.line);
    }
    var cell = yield* resolveCell(node.target, ctx);
    if (ctx.lang === 'cpp') {
      val = cppCoerce(cell.vtype, val, node.line);
    } else if (!cell.init) {
      ctx.pendingInsight = {
        kind: 'bind',
        text: node.target.name + ' — 이 이름은 대입이 실행되는 순간에 만들어집니다. '
          + '이 줄을 지나기 전까지는 네임스페이스에 아예 없어요.'
      };
    }
    cell.value = val; cell.init = true; cell.fresh = false;
    return val;
  }

  var PY_BUILTINS = { print: 1, range: 1, len: 1, int: 1, float: 1, str: 1, bool: 1, abs: 1, type: 1 };

  function* doCall(node, ctx) {
    var callee = node.callee;
    var name = callee.type === 'Name' ? callee.name : null;

    // 파이썬 내장
    if (ctx.lang === 'py' && name && PY_BUILTINS[name] && !Object.prototype.hasOwnProperty.call(ctx.globals, name)) {
      var args = [];
      for (var i = 0; i < node.args.length; i++) args.push(yield* evalExpr(node.args[i], ctx));
      return callBuiltin(name, args, node.line, ctx);
    }

    var fn = null;
    if (ctx.lang === 'cpp') {
      fn = ctx.funcs[name];
      if (!fn) throw rt(node.line, 'C3861', "'" + name + "': 식별자를 찾을 수 없습니다.");
    } else {
      var fv = yield* evalExpr(callee, ctx);
      if (fv.t !== 'func') throw rt(node.line, 'TypeError', "'" + pyTypeName(fv) + "' object is not callable");
      fn = fv.v;
    }

    var argv = [];
    for (var j = 0; j < node.args.length; j++) argv.push(yield* evalExpr(node.args[j], ctx));
    if (argv.length !== fn.params.length) {
      throw rt(node.line, ctx.lang === 'cpp' ? 'C2660' : 'TypeError',
        fn.name + '() — 인자 ' + fn.params.length + '개를 받는데 ' + argv.length + '개를 넘겼습니다.');
    }

    if (ctx.frames.length > 40) throw rt(node.line, ctx.lang === 'cpp' ? 'C1001' : 'RecursionError', '호출이 너무 깊습니다 (무한 재귀?).');

    var frame = newFrame(fn.name + '()', ctx.lang, fn.line);
    frame.isGlobal = false;
    frame.callLine = node.line;
    if (ctx.lang === 'py') {
      frame.localNames = fn.localNames;
      frame.globalNames = fn.globalNames;
    }
    for (var k = 0; k < fn.params.length; k++) {
      var p = fn.params[k];
      var v = ctx.lang === 'cpp' ? cppCoerce(p.vtype, argv[k], node.line) : argv[k];
      declareVar(frame, p.name, { name: p.name, value: v, init: true, vtype: ctx.lang === 'cpp' ? p.vtype : null, isParam: true });
    }
    ctx.frames.push(frame);

    yield pause(fn.line, fn.name + '() 안으로 들어왔습니다.', {
      insight: ctx.lang === 'cpp'
        ? '컴파일 시점에 이미 이름 검사는 끝났습니다. 지금 새로 생기는 건 스택 프레임(매개변수/지역 변수)뿐이에요.'
        : '함수 몸체는 지금 처음으로 "실행"됩니다. 안에서 쓰는 이름들도 지금 이 순간에 찾기 시작해요.',
      enter: true
    });

    var ret = null;
    try {
      yield* execBlockBody(fn.body.body, ctx, frame);
      ret = ctx.lang === 'cpp' ? V('void', undefined) : V('none', null);
    } catch (e) {
      if (e && e.__ret) ret = e.value;
      else throw e;                       // 오류로 멈춘 프레임은 남겨서 상태를 볼 수 있게 한다
    }
    if (ctx.lang === 'cpp' && fn.vtype && fn.vtype !== 'void' && ret && ret.t !== 'void') {
      ret = cppCoerce(fn.vtype, ret, node.line);
    }
    ctx.frames.pop();
    yield pause(node.line, fn.name + '() 에서 돌아왔습니다.', {
      insight: ctx.lang === 'cpp'
        ? '프레임이 정리되면서 이 함수의 지역 변수는 전부 사라집니다.'
        : '함수 프레임이 사라집니다. 함수 안에서 만든 지역 이름들도 같이 없어져요.',
      leave: true,
      retval: ret && ret.t !== 'void' && ret.t !== 'none' ? (ctx.lang === 'cpp' ? fmtCpp(ret) : fmtPy(ret)) : null
    });
    return ret;
  }

  function callBuiltin(name, args, line, ctx) {
    switch (name) {
      case 'print': {
        var parts = [];
        for (var i = 0; i < args.length; i++) parts.push(toStrOut(args[i], 'py'));
        ctx.out.push(parts.join(' ') + '\n');
        return V('none', null);
      }
      case 'range': {
        var a = args.length > 1 ? numOf2(args[0]) : 0;
        var b = args.length > 1 ? numOf2(args[1]) : numOf2(args[0] || V('int', 0));
        var st = args.length > 2 ? numOf2(args[2]) : 1;
        if (st === 0) throw rt(line, 'ValueError', 'range() arg 3 must not be zero');
        var arr = [];
        for (var v = a; st > 0 ? v < b : v > b; v += st) {
          arr.push(V('int', v));
          if (arr.length > 10000) break;
        }
        return V('list', arr, { isRange: true, rangeArgs: args.map(function (x) { return numOf2(x); }) });
      }
      case 'len': {
        var t = args[0];
        if (!t || (t.t !== 'list' && t.t !== 'str')) throw rt(line, 'TypeError', "object of type '" + pyTypeName(t) + "' has no len()");
        return V('int', t.t === 'list' ? t.v.length : String(t.v).length);
      }
      case 'int': {
        var x = args[0];
        if (x.t === 'str') {
          var n = parseInt(x.v, 10);
          if (isNaN(n)) throw rt(line, 'ValueError', "invalid literal for int() with base 10: '" + x.v + "'");
          return V('int', n);
        }
        return V('int', Math.trunc(numOf2(x)));
      }
      case 'float': return V('float', x2f(args[0], line));
      case 'str': return V('str', toStrOut(args[0], 'py'));
      case 'bool': return V('bool', truthy(args[0], 'py'));
      case 'abs': return V(args[0].t === 'float' ? 'float' : 'int', Math.abs(numOf2(args[0])));
      case 'type': return V('str', "<class '" + pyTypeName(args[0]) + "'>");
      default: throw rt(line, 'NameError', "name '" + name + "' is not defined");
    }
  }
  function x2f(v, line) {
    if (v.t === 'str') {
      var n = parseFloat(v.v);
      if (isNaN(n)) throw rt(line, 'ValueError', "could not convert string to float: '" + v.v + "'");
      return n;
    }
    return numOf2(v);
  }

  /* ---------- 문 실행 ---------- */
  function* execBlockBody(stmts, ctx, frame) {
    for (var i = 0; i < stmts.length; i++) yield* execStmt(stmts[i], ctx, frame);
  }

  function* execStmt(s, ctx, frame) {
    ctx.steps++;
    if (ctx.steps > MAX_STEPS * 3) throw rt(s.line, '', '실행 단계가 너무 많습니다 (무한 루프?).');
    var lang = ctx.lang;

    switch (s.type) {
      case 'Nop': return;

      case 'FuncDecl': {
        if (lang === 'cpp') return;                          // C++ 함수는 실행 대상이 아님
        yield pause(s.line, 'def ' + s.name + ' — 함수 객체를 만들고 이름을 붙입니다.', {
          insight: 'def 줄도 실행되는 문장입니다. 이 줄을 지나야 ' + s.name + ' 이 이름으로 존재해요. '
            + '몸체 안의 코드는 아직 한 줄도 검사되지 않았습니다.'
        });
        s.localNames = s.localNames || collectPyLocals(s);
        s.globalNames = s.globalNames || collectPyGlobals(s);
        var cell = yield* resolveCell({ type: 'Name', name: s.name, line: s.line }, ctx);
        cell.value = V('func', s); cell.init = true;
        return;
      }

      case 'VarDecl': {
        var names = [];
        for (var i = 0; i < s.decls.length; i++) names.push(s.decls[i].name);
        var hasInit = s.decls.some(function (d) { return !!d.init; });
        yield pause(s.line, s.vtype + ' ' + names.join(', ') + (hasInit ? ' — 선언과 동시에 값이 들어갑니다.' : ' — 선언만, 값은 아직 없습니다.'), {
          insight: hasInit ? null
            : (s.vtype === 'string'
              ? 'std::string 은 클래스라 기본 생성자가 불립니다. 쓰레기 값이 아니라 빈 문자열("")이 돼요.'
              : '초기화가 없으면 스택에 자리만 잡습니다. 이 줄을 지나도 값은 쓰레기 그대로예요.')
        });
        for (var j = 0; j < s.decls.length; j++) {
          var d = s.decls[j];
          var val = null, init = false;
          if (d.init) {
            val = cppCoerce(s.vtype, yield* evalExpr(d.init, ctx), d.line);
            init = true;
          } else {
            val = cppDefault(s.vtype);
            init = val !== null;
          }
          declareVar(frame, d.name, {
            name: d.name, value: val, init: init,
            vtype: s.vtype === 'auto' && val ? val.t : s.vtype,
            declLine: d.line
          });
        }
        return;
      }

      case 'Assign': {
        var isNew = lang === 'py' && !pyHasName(ctx, s.target.name);
        yield pause(s.line, (s.target.name || '변수') + (lang === 'py' && isNew
          ? ' — 이 대입으로 이름이 처음 생깁니다.'
          : ' 에 값을 대입합니다.'));
        yield* doAssign(s, ctx);
        return;
      }

      case 'ExprStmt': {
        yield pause(s.line, describeExpr(s.expr, lang));
        yield* evalExpr(s.expr, ctx);
        return;
      }

      case 'Out': {
        yield pause(s.line, '값을 출력합니다.');
        var buf = '';
        for (var k = 0; k < s.parts.length; k++) {
          var part = s.parts[k];
          if (part.type === 'Endl') { buf += '\n'; continue; }
          buf += toStrOut(yield* evalExpr(part, ctx), 'cpp');
        }
        ctx.out.push(buf);
        return;
      }

      case 'Return': {
        yield pause(s.line, s.value ? '값을 계산해서 돌려줍니다.' : '반환하고 함수를 빠져나갑니다.');
        var rv = s.value ? yield* evalExpr(s.value, ctx) : (lang === 'cpp' ? V('void', undefined) : V('none', null));
        throw new ReturnSig(rv);
      }

      case 'Break': yield pause(s.line, '반복문을 빠져나갑니다.'); throw { __brk: true };
      case 'Continue': yield pause(s.line, '다음 반복으로 건너뜁니다.'); throw { __cont: true };

      case 'Global': {
        yield pause(s.line, 'global 선언 — 이 이름들은 전역을 가리킵니다.');
        return;
      }

      case 'If': {
        yield pause(s.line, '조건을 검사합니다.');
        var c = yield* evalExpr(s.cond, ctx);
        var taken = truthy(c, lang);
        if (taken) yield* execBranch(s.then, ctx, frame);
        else if (s.alt) yield* execBranch(s.alt, ctx, frame);
        return;
      }

      case 'While': {
        for (;;) {
          yield pause(s.line, '반복 조건을 검사합니다.');
          var cv = yield* evalExpr(s.cond, ctx);
          if (!truthy(cv, lang)) break;
          try {
            yield* execBranch(s.body, ctx, frame);
          } catch (e) {
            if (e && e.__brk) break;
            if (e && e.__cont) continue;
            throw e;
          }
        }
        return;
      }

      case 'For': {                                     // C++ for
        frame.scopes.push(Object.create(null));
        var declared = [];
        try {
          if (s.init) {
            yield* execStmt(s.init, ctx, frame);
            if (s.init.type === 'VarDecl') {
              for (var q = 0; q < s.init.decls.length; q++) declared.push(s.init.decls[q].name);
            }
          }
          for (;;) {
            yield pause(s.line, '반복 조건을 검사합니다.');
            if (s.cond) {
              var fc = yield* evalExpr(s.cond, ctx);
              if (!truthy(fc, 'cpp')) break;
            }
            var brk = false;
            try {
              yield* execBranch(s.body, ctx, frame);
            } catch (e2) {
              if (e2 && e2.__brk) brk = true;
              else if (e2 && e2.__cont) { /* 증감으로 진행 */ }
              else throw e2;
            }
            if (brk) break;
            if (s.step) {
              yield pause(s.line, '반복 변수를 증감시킵니다.');
              yield* evalExpr(s.step, ctx);
            }
          }
        } finally {
          frame.scopes.pop();
        }
        if (declared.length) {
          yield pause(s.line, '반복문이 끝났습니다.', {
            insight: 'for 안에서 선언한 ' + declared.join(', ') + ' — 반복문 스코프와 함께 사라집니다. 이 아래에서 쓰면 C2065 예요.'
          });
        }
        return;
      }

      case 'ForIn': {                                   // Python for
        yield pause(s.line, '반복할 값들을 준비합니다.');
        var it = yield* evalExpr(s.iter, ctx);
        if (it.t !== 'list' && it.t !== 'str') throw rt(s.line, 'TypeError', "'" + pyTypeName(it) + "' object is not iterable");
        var seq = it.t === 'list' ? it.v : String(it.v).split('').map(function (ch) { return V('str', ch); });
        for (var idx = 0; idx < seq.length; idx++) {
          var loopCell = yield* resolveCell({ type: 'Name', name: s.name, line: s.line }, ctx);
          loopCell.value = seq[idx]; loopCell.init = true;
          yield pause(s.line, s.name + ' = ' + fmtPy(seq[idx]) + ' — 이번 회차를 시작합니다.', idx === 0 ? {
            insight: '반복 변수 ' + s.name + ' — 그냥 대입일 뿐이라 반복문이 끝나도 마지막 값이 그대로 남습니다.'
          } : null);
          try {
            yield* execBranch(s.body, ctx, frame);
          } catch (e3) {
            if (e3 && e3.__brk) break;
            if (e3 && e3.__cont) continue;
            throw e3;
          }
        }
        return;
      }

      case 'Block': {
        frame.scopes.push(Object.create(null));
        var declaredHere = [];
        try {
          for (var b = 0; b < s.body.length; b++) {
            if (s.body[b].type === 'VarDecl') {
              for (var c2 = 0; c2 < s.body[b].decls.length; c2++) declaredHere.push(s.body[b].decls[c2].name);
            }
          }
          yield* execBlockBody(s.body, ctx, frame);
        } finally {
          frame.scopes.pop();
        }
        if (declaredHere.length && ctx.lang === 'cpp') {
          yield pause(s.endLine, '블록이 끝났습니다.', {
            insight: '이 닫는 중괄호를 지나면 블록 안에서 선언한 ' + declaredHere.join(', ') + ' — 여기서 소멸합니다. 아래에서 쓰면 C2065 예요.'
          });
        }
        return;
      }

      default:
        throw rt(s.line, '', '지원하지 않는 문장입니다. (' + s.type + ')');
    }
  }

  function* execBranch(node, ctx, frame) {
    if (!node) return;
    if (node.type === 'Block') { yield* execStmt(node, ctx, frame); return; }
    yield* execStmt(node, ctx, frame);
  }

  function describeExpr(e, lang) {
    if (e.type === 'Call' && e.callee.type === 'Name') {
      if (lang === 'py' && e.callee.name === 'print') return '값을 출력합니다.';
      return e.callee.name + '() 호출 — 함수 안으로 들어갑니다.';
    }
    if (e.type === 'Assign') return (e.target.name || '변수') + ' 에 값을 대입합니다.';
    if (e.type === 'IncDec') return (e.target.name || '변수') + ' — ' + (e.op === '++' ? '1 증가' : '1 감소') + '시킵니다.';
    return '이 식을 계산합니다.';
  }

  function pyHasName(ctx, name) {
    if (!name) return true;
    var f = ctx.frames[ctx.frames.length - 1];
    if (!f.isGlobal && Object.prototype.hasOwnProperty.call(f.scopes[0], name) && f.scopes[0][name].init) return true;
    return Object.prototype.hasOwnProperty.call(ctx.globals, name) && ctx.globals[name].init;
  }

  /* ---------- 파이썬 지역 이름 수집 ---------- */
  function collectPyLocals(fn) {
    var set = Object.create(null);
    for (var i = 0; i < fn.params.length; i++) set[fn.params[i].name] = true;
    (function walk(nodes) {
      for (var i = 0; i < nodes.length; i++) {
        var s = nodes[i];
        if (!s) continue;
        if (s.type === 'Assign' && s.target.type === 'Name') set[s.target.name] = true;
        if (s.type === 'ForIn') { set[s.name] = true; walk(s.body.body); }
        if (s.type === 'FuncDecl') set[s.name] = true;
        if (s.type === 'If') { walk(s.then.body || [s.then]); if (s.alt) walk(s.alt.body || [s.alt]); }
        if (s.type === 'While') walk(s.body.body || [s.body]);
        if (s.type === 'Block') walk(s.body);
      }
    })(fn.body.body);
    return set;
  }
  function collectPyGlobals(fn) {
    var set = Object.create(null);
    (function walk(nodes) {
      for (var i = 0; i < nodes.length; i++) {
        var s = nodes[i];
        if (!s) continue;
        if (s.type === 'Global') for (var j = 0; j < s.names.length; j++) set[s.names[j]] = true;
        if (s.type === 'If') { walk(s.then.body || [s.then]); if (s.alt) walk(s.alt.body || [s.alt]); }
        if (s.type === 'While') walk(s.body.body || [s.body]);
        if (s.type === 'ForIn') walk(s.body.body);
        if (s.type === 'Block') walk(s.body);
      }
    })(fn.body.body);
    return set;
  }

  /* =============================================================
     6. 스냅샷
     ============================================================= */
  function cellView(cell, lang) {
    var state, display, badge = null;
    if (lang === 'cpp') {
      if (!cell.init) {
        state = 'garbage';
        display = fmtCpp(V(cell.vtype, garbageFor(cell.vtype)));
        badge = '쓰레기 값';
      } else {
        state = 'value';
        display = fmtCpp(cell.value);
      }
      return { name: cell.name, type: cell.vtype, state: state, display: display, badge: badge, param: !!cell.isParam };
    }
    return {
      name: cell.name,
      type: pyTypeName(cell.value),
      state: 'value',
      display: fmtPy(cell.value),
      badge: null,
      param: !!cell.isParam
    };
  }

  function snapshotFrames(ctx) {
    var views = [];
    for (var i = 0; i < ctx.frames.length; i++) {
      var f = ctx.frames[i];
      var vars = [];
      for (var s = 0; s < f.scopes.length; s++) {
        var sc = f.scopes[s];
        for (var name in sc) {
          if (ctx.lang === 'py' && !sc[name].init) continue;
          var v = cellView(sc[name], ctx.lang);
          v.depth = s;
          vars.push(v);
        }
      }
      views.push({ title: f.name, vars: vars, isGlobal: !!f.isGlobal, active: i === ctx.frames.length - 1 });
    }
    if (ctx.lang === 'py') {
      var g = [];
      for (var n in ctx.globals) {
        if (!ctx.globals[n].init) continue;
        var gv = cellView(ctx.globals[n], 'py');
        gv.depth = 0;
        g.push(gv);
      }
      views[0] = { title: '전역 (module)', vars: g, isGlobal: true, active: ctx.frames.length === 1 };
    }
    return views;
  }

  /* =============================================================
     7. 공개 API
     ============================================================= */
  function analyze(src, lang) {
    var result = {
      lang: lang, source: src, lines: src.replace(/\r\n?/g, '\n').split('\n'),
      steps: [], compileErrors: [], parseError: null, truncated: false
    };
    var ast;
    try {
      ast = parse(src, lang);
    } catch (e) {
      if (e && e.__ce) {
        result.parseError = { line: e.line, code: e.code, msg: e.msg };
        return result;
      }
      throw e;
    }

    if (lang === 'cpp') {
      result.compileErrors = checkCpp(ast);
      if (result.compileErrors.length) return result;        // 컴파일 실패 → 실행 자체가 없다
    }

    var ctx = makeCtx(lang);
    var globalFrame = newFrame(lang === 'cpp' ? '전역' : '전역 (module)', lang, 1);
    globalFrame.isGlobal = true;
    ctx.globals = globalFrame.scopes[0];
    ctx.frames.push(globalFrame);

    // C++ 함수 등록 (컴파일 시점에 이미 알려진다)
    if (lang === 'cpp') {
      for (var i = 0; i < ast.body.length; i++) {
        if (ast.body[i].type === 'FuncDecl') ctx.funcs[ast.body[i].name] = ast.body[i];
      }
    }

    var steps = result.steps;
    function record(p) {
      if (ctx.pendingInsight) {
        // 방금 실행된 줄이 만든 설명이므로, 그 줄을 가리키던 단계에 붙인다
        var prev = steps[steps.length - 1];
        if (prev && !prev.insight) prev.insight = ctx.pendingInsight.text;
        ctx.pendingInsight = null;
      }
      steps.push({
        line: p.line,
        note: p.note,
        insight: p.insight || null,
        enter: !!p.enter,
        leave: !!p.leave,
        retval: p.retval || null,
        frames: snapshotFrames(ctx),
        out: ctx.out.join(''),
        error: null,
        end: false
      });
    }

    var gen = (function* () {
      if (lang !== 'cpp') {
        yield* execBlockBody(ast.body, ctx, globalFrame);
        return;
      }
      // C++: 전역 변수 초기화가 먼저, 그다음 main()
      for (var g = 0; g < ast.body.length; g++) {
        if (ast.body[g].type !== 'FuncDecl') yield* execStmt(ast.body[g], ctx, globalFrame);
      }
      var mainFn = ctx.funcs['main'];
      if (!mainFn) return;
      var mainFrame = newFrame('main()', 'cpp', mainFn.line);
      mainFrame.isGlobal = false;
      ctx.frames.push(mainFrame);
      yield pause(mainFn.line, 'main() 이 시작됩니다.', {
        enter: true,
        insight: '컴파일이 통과했다는 건 이름 검사가 이미 전부 끝났다는 뜻입니다. 지금부터는 값이 채워지는 과정만 남았어요.'
      });
      try {
        yield* execBlockBody(mainFn.body.body, ctx, mainFrame);
      } catch (e) {
        if (!(e && e.__ret)) throw e;     // main() 프레임도 마찬가지
      }
      ctx.frames.pop();
    })();

    try {
      for (;;) {
        var r = gen.next();
        if (r.done) break;
        record(r.value);
        if (steps.length >= MAX_STEPS) { result.truncated = true; break; }
      }
      if (ctx.pendingInsight && steps.length && !steps[steps.length - 1].insight) {
        steps[steps.length - 1].insight = ctx.pendingInsight.text;
        ctx.pendingInsight = null;
      }
      if (!result.truncated) {
        steps.push({
          line: null, note: '프로그램이 끝났습니다.', insight: null,
          frames: snapshotFrames(ctx), out: ctx.out.join(''), error: null, end: true
        });
      }
    } catch (e) {
      if (e && e.__rt) {
        if (ctx.pendingInsight && steps.length && !steps[steps.length - 1].insight) {
          steps[steps.length - 1].insight = ctx.pendingInsight.text;
          ctx.pendingInsight = null;
        }
        // 오류 발생 직전 상태를 그대로 두고 오류 단계를 덧붙인다
        steps.push({
          line: e.line, note: '여기서 멈췄습니다.', insight: null,
          frames: snapshotFrames(ctx), out: ctx.out.join(''),
          error: { line: e.line, code: e.code, msg: e.msg }, end: true
        });
      } else if (e && e.__ret) {
        steps.push({
          line: null, note: '프로그램이 끝났습니다.', insight: null,
          frames: snapshotFrames(ctx), out: ctx.out.join(''), error: null, end: true
        });
      } else if (e && e.__ce) {
        result.parseError = { line: e.line, code: e.code, msg: e.msg };
      } else throw e;
    }
    return result;
  }

  root.CSVEngine = {
    analyze: analyze,
    parse: parse,
    checkCpp: checkCpp,
    MAX_STEPS: MAX_STEPS
  };

})(typeof window !== 'undefined' ? window : globalThis);
