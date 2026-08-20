/* =============================================================
   예제 모음 — 왼쪽 C++ / 오른쪽 Python 을 같은 의도로 짝지었다.
   ============================================================= */
(function (root) {
  'use strict';

  var EXAMPLES = [
    {
      id: 'order',
      title: '이름을 먼저 쓰면',
      lede: '선언(대입)보다 먼저 이름을 쓰면 어디서 걸리나',
      point: 'C++은 컴파일러가 위에서 아래로 읽다가 그 자리에서 막습니다 — 실행은 시작조차 안 해요. '
        + 'Python은 일단 돌다가 그 줄에 도달하는 순간 NameError 로 죽습니다.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    cout << msg << endl;   // 아직 msg 가 없다\n' +
        '\n' +
        '    string msg = "hello";\n' +
        '    cout << msg << endl;\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'print(msg)      # 아직 msg 가 없다\n' +
        '\n' +
        'msg = "hello"\n' +
        'print(msg)\n'
    },
    {
      id: 'funcbody',
      title: '함수 몸체는 언제 검사되나',
      lede: '함수 안에서 아래쪽 전역 이름을 쓰면',
      point: '이게 핵심 차이입니다. C++ 함수 몸체는 정의하는 그 자리에서 이름이 다 있어야 합니다. '
        + 'Python 함수 몸체는 정의할 때 실행되지 않고, 실제로 호출되는 순간에야 이름을 찾습니다.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'void show()\n' +
        '{\n' +
        '    cout << msg << endl;   // 정의 시점에 msg 가 있어야 한다\n' +
        '}\n' +
        '\n' +
        'string msg = "polar";\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    show();\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'def show():\n' +
        '    print(msg)      # 이 줄은 지금 검사되지 않는다\n' +
        '\n' +
        'msg = "polar"       # 호출 전에만 만들어지면 된다\n' +
        '\n' +
        'show()\n'
    },
    {
      id: 'uninit',
      title: '초기화 안 한 지역 변수',
      lede: '중단점을 선언 줄 바로 다음에 찍었을 때 보이는 값',
      point: 'C++ 지역 변수는 선언만 하면 스택 자리만 잡고 값은 그대로 쓰레기입니다. '
        + 'Visual Studio 디버그 빌드는 0xCC 로 채워서 int 가 -858993460 으로 보이죠. '
        + 'Python에는 "선언만" 이라는 상태가 없어서, 대입 전에는 이름 자체가 없습니다.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    int x;            // 선언만\n' +
        '    string s;         // string 은 클래스라 기본 생성\n' +
        '\n' +
        '    cout << x << endl;\n' +
        '\n' +
        '    x = 5;            // 이 줄을 지나야 5 가 보인다\n' +
        '    cout << x << endl;\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'x = None      # 파이썬에는 "선언만" 이 없다\n' +
        'print(x)\n' +
        '\n' +
        'x = 5         # 이 줄을 지나야 5 가 보인다\n' +
        'print(x)\n'
    },
    {
      id: 'blockscope',
      title: '중괄호가 만드는 수명',
      lede: 'if / 블록 안에서 만든 변수가 밖에서도 살아있나',
      point: 'C++은 블록이 스코프입니다. } 를 지나면 그 안에서 선언한 변수는 소멸해요. '
        + 'Python의 if/for 는 스코프를 만들지 않습니다 — 함수만 스코프예요.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    int total = 0;\n' +
        '\n' +
        '    {\n' +
        '        int t = 3;\n' +
        '        total = total + t;\n' +
        '    }\n' +
        '\n' +
        '    cout << total << endl;\n' +
        '    // cout << t << endl;   // 주석을 풀면 C2065: t 는 이미 없다\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'total = 0\n' +
        '\n' +
        'if True:\n' +
        '    t = 3\n' +
        '    total = total + t\n' +
        '\n' +
        'print(total)\n' +
        'print(t)      # if 는 스코프를 만들지 않아서 살아있다\n'
    },
    {
      id: 'loopvar',
      title: '반복 변수의 수명',
      lede: '반복문이 끝난 뒤 i 를 찍어보면',
      point: 'for(int i...) 의 i 는 반복문 스코프 소속이라 루프가 끝나면 사라집니다. '
        + 'Python의 for 변수는 그냥 대입이라 끝난 뒤에도 마지막 값이 남아요.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    int sum = 0;\n' +
        '\n' +
        '    for (int i = 0; i < 3; i++)\n' +
        '    {\n' +
        '        sum += i;\n' +
        '    }\n' +
        '\n' +
        '    cout << sum << endl;\n' +
        '    // cout << i << endl;   // 주석을 풀면 C2065: i 는 루프와 함께 사라졌다\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'total = 0\n' +
        '\n' +
        'for i in range(3):\n' +
        '    total += i\n' +
        '\n' +
        'print(total)\n' +
        'print(i)      # 마지막 값 2 가 그대로 남아있다\n'
    },
    {
      id: 'localshadow',
      title: '함수 안에서 전역을 건드리면',
      lede: '같은 이름에 대입하는 순간 무엇이 달라지나',
      point: 'C++은 지역 선언과 전역 참조가 문법으로 갈립니다. '
        + 'Python은 함수 안에 대입이 하나라도 있으면 그 이름 전체가 지역으로 취급돼, 읽기만 하던 줄까지 UnboundLocalError 가 됩니다.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int count = 10;\n' +
        '\n' +
        'void bump()\n' +
        '{\n' +
        '    int count = 0;    // 전역을 가리는 새 지역 변수\n' +
        '    count = count + 1;\n' +
        '    cout << count << endl;\n' +
        '}\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    bump();\n' +
        '    cout << count << endl;\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'count = 10\n' +
        '\n' +
        'def bump():\n' +
        '    print(count)     # 읽기만 하는데도 지역으로 본다\n' +
        '    count = count + 1\n' +
        '\n' +
        'bump()\n' +
        'print(count)\n'
    },
    {
      id: 'division',
      title: '같은 식, 다른 결과',
      lede: '7 / 2 와 타입이 정해지는 시점',
      point: 'C++은 변수의 타입이 선언에 박혀 있어서 int/int 는 소수점을 버립니다. '
        + 'Python은 값이 타입을 들고 다녀서 / 는 항상 float, 버리려면 // 를 씁니다.',
      cpp:
        '#include <iostream>\n' +
        'using namespace std;\n' +
        '\n' +
        'int main()\n' +
        '{\n' +
        '    int a = 7;\n' +
        '    int b = 2;\n' +
        '\n' +
        '    int r1 = a / b;\n' +
        '    double r2 = a / b;        // 나눈 뒤에 옮겨담아도 늦었다\n' +
        '    double r3 = (double)a / b;\n' +
        '\n' +
        '    cout << r1 << " " << r2 << " " << r3 << endl;\n' +
        '    return 0;\n' +
        '}\n',
      py:
        'a = 7\n' +
        'b = 2\n' +
        '\n' +
        'r1 = a // b\n' +
        'r2 = a / b        # 항상 float\n' +
        'r3 = float(a) / b\n' +
        '\n' +
        'print(r1, r2, r3)\n'
    }
  ];

  root.CSVExamples = EXAMPLES;
})(typeof window !== 'undefined' ? window : globalThis);
