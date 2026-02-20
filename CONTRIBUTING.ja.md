# Adaptive AUTOSAR Linux シミュレーターへの貢献

このドキュメントは、このプロジェクトへの貢献方法を案内します。プロジェクトの行動規範については
[こちらのドキュメント](https://github.com/langroodi/Adaptive-AUTOSAR/blob/master/CODE_OF_CONDUCT.md)
をご参照ください。

## 目次

[貢献者のマインドセット](#貢献者のマインドセット)

[貢献の理由と方法](#貢献の理由と方法)

- [バグを見つけた場合](#バグを見つけた場合)
- [新しい機能・ユニットテストの追加または既存の拡張](#新しい機能ユニットテストの追加または既存の拡張)
- [貢献したいが方法がわからない場合](#貢献したいが方法がわからない場合)

[コード規約（C++）](#コード規約c)

- [レイアウト規約](#レイアウト規約)
- [命名規約](#命名規約)
- [言語ガイドライン](#言語ガイドライン)

[メンテナンス](#メンテナンス)

- [ドキュメント](#ドキュメント)
- [ユニットテスト](#ユニットテスト)
- [外部依存関係](#外部依存関係)
- [コミットメッセージ（オプション）](#コミットメッセージオプション)

## 貢献者のマインドセット

まず、このプロジェクトに貢献に興味を持っていただきありがとうございます。
このプロジェクトへの貢献を始めるための 7 つのヒントをご紹介します:

1. **楽しんでください:**

> まだ世界で最も嫌いなものがわからないなら、C++ コードを書いたことがないということです。
C++ でこのプロジェクトを開発するのは苦労することもありますが、楽しくもあります
（見方によって変わります）。
貢献中にフラストレーションを感じたら、一息ついてフレッシュな気持ちでコードに戻りましょう。
そして、コーディング中は常に笑顔でいてください（本当に！良質なコードを生み出すのに役立ちます）。

2. **批判的に考えてください:**

> 最初に思い浮かんだ解決策が必ずしも最良の解決策とは限りません。
開発の目的はコードの一部を完成させることではなく、昨日より今日の方が良いコードを
書く方法を学ぶことです。
開発を始めた当日に最良の解決策を見つけることは難しいですが、
改善の余地があるかどうかを数分間考えることは常に価値があります。
また、変更をコミットする前に必ず再確認することを強くお勧めします。

3. **推測しないでください:**

> コードブロックがどのように動作するかわからない場合は、推測しないでください。
テストし、調べるか、質問してください（例: [プロジェクトのディスカッションボード](https://github.com/langroodi/Adaptive-AUTOSAR/discussions)）。
コードスニペットを自分のコードにコピー&ペーストする前に、そのスニペットの背後にある
ロジックを理解しようとしてください。
動作原理を知らずにプロジェクトでメカニズム（プロトコル、パッケージなど）を採用しないでください。

4. **ドキュメント化してください:**

> 素晴らしいアイデアをコードの出発点として見つけたら、後で手遅れになる前に
短い説明を書いておくと良いでしょう。
開発フェーズ中にそのアイデアが変わる可能性があると思うなら、
それに応じて書いたドキュメントを修正すれば大丈夫です。
これらのドキュメントは [プロジェクト Wiki](https://github.com/langroodi/Adaptive-AUTOSAR/wiki) に置けます。
自分のためではなく、見知らぬ人が読む可能性があるように書いてください。

5. **リファクタリングしてください:**

> 特定のプロジェクトクラスから複数の問題が発生したり、そのクラスの問題のデバッグが
難しいと感じたりしたら、パッチを当てるのではなくリファクタリングする時です。
今すぐリファクタリングする時間がない場合は、新しい
[メンテナンス Issue](https://github.com/langroodi/Adaptive-AUTOSAR/issues/new?assignees=&labels=enhancement&template=feature_request.md&title=)
を作成して、後で取り組むことを忘れないようにしてください。

6. **ポータブルにしてください:**

> [プロジェクトの依存関係](https://github.com/langroodi/Adaptive-AUTOSAR#dependencies) を尊重し、
ソフトウェアおよびハードウェアの両面で、自分のローカルシステムでのみ動作する可能性がある
命令の使用を避けてください。
例えば、AVX512 組み込み命令を使用すると、コードが特定のハードウェア要件に依存します。

7. **読みやすく保ってください:**

> 賢くなろうとして、自分以外誰も理解できないコードを書くことを避けてください
（「高速逆数平方根」のようなもの）。
パフォーマンスの観点からそれが避けられない場合は、コードにコメントを付けて
あなたの賢いアイデアを他の人に説明してください。

## 貢献の理由と方法

このプロジェクトへの貢献の理由に関わらず、貢献のシンプルなルールはリポジトリをフォークし、
変更を加えて、[プルリクエスト](https://github.com/langroodi/Adaptive-AUTOSAR/pulls) を作成することです。
以下のような様々な理由でこのプロジェクトに貢献できます:

### バグを見つけた場合

このプロジェクトにバグを見つけた場合、従うべきセキュリティポリシーはありません。
[Issues](https://github.com/langroodi/Adaptive-AUTOSAR/issues/new?assignees=langroodi&labels=bug&template=bug_report.md&title=)
からバグを報告してください。
バグを修正したい場合は、Issue を作成した後、自分のフォーク上で修正し、
Issue を [メンション](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue) した状態でプルリクエストを作成してください。

### 新しい機能・ユニットテストの追加または既存の拡張

このプロジェクトに新しい機能・ユニットテストの追加や既存のものの拡張に興味がある場合は、
まず新しい [ディスカッション](https://github.com/langroodi/Adaptive-AUTOSAR/discussions/new) を作成して、
他のコミュニティメンバーがそれに興味を持つかどうかを確認してください。
コミュニティの大多数が同意した場合、そこから新しい Issue を作成でき、残りの道は明確になるでしょう。

### 貢献したいが方法がわからない場合

プロジェクトに興味はあるが貢献方法がわからない場合、最初に確認すべき場所は
[Issues ボード](https://github.com/langroodi/Adaptive-AUTOSAR/issues) です。
適切なものが見つからない場合は、新しい [ディスカッション](https://github.com/langroodi/Adaptive-AUTOSAR/discussions/new)
を開始してください。コミュニティが適切なタスクを見つけようとします。
コードのフォーマット修正、タイポ修正、コードへのコメント追加によって貢献しないでください。
これらの種類の貢献に関連する PR は拒否されます。

## コード規約（C++）

すべてのプロジェクト貢献者で一貫したコーディングスタイルを保つために、以下のドメインの規約をお読みください:

### レイアウト規約

- 列の制限を 80 文字に保つよう努めてください。

- 中括弧を別の新しい行に置いてください（[Allman または BSD スタイル](https://en.wikipedia.org/wiki/Indentation_style#Allman_style)）:
``` c++
if (a > b)
{

}
```

- 各行に複数の文を置くことを避けてください。

### 命名規約

- クラス、構造体、protected/public メンバー/メソッド、テンプレートパラメーター名は `PascalCase` にする:

``` c++
template <typename TParam>
class MyClass
{
protected:
  int Member;
public:
  void Method();
};

struct MyStruct
{
  int Member;
};
```

- 名前空間は `lowercase` にする:
``` c++
namespace myapplication
{
  namespace myclasses
  {
    // ...
  }
}
```

- クラスまたは構造体のファイル名は、`PascalCase` 名を分割して `lowercase` のセクションを `_` で再結合することで生成する。
例えば、`MyClass` のヘッダーファイル名は `my_class.h` になります。

- プライベートメンバー/メソッド、関数引数、ローカル変数名は `camelCase` にする:

``` c++
template <typename TParam>
class MyClass
{
private:
  void method(int argument)
  {
  }
};
```

- ポインター名の後に `_ptr` サフィックスを追加するなど、変数の型を名前で示す必要はありません。

- 静的定数、プライベートメンバー、ローカル変数名はそれぞれ `c`、`m`、`_` プレフィックスを持つ:

``` c++
class MyClass
{
private:
  static const int cNumber;
  int mNumber;

  void method(int argument)
  {
    int _temp = argument;
  }
};
```

- 配列やその他のコンテナ型（例: `std::vector`）には複数形の名前を使用する:
``` c++
std::array<int> numbers
std::vector<char> characters;
```

### 言語ガイドライン

- `auto` キーワードはイテレーターベースのループで使用するか、型が明示的に言及されている場合のみ使用する:

``` c++
auto foo = std::make_shared<int>(1);
for(auto number : numbers)
{
  // 次の行では 'auto' キーワードの使用を避けた方が良い:
  int _result = GetResult();
}
```

- 古い C/C++ の代替である `typedef` の代わりに `using` キーワードなど C++11/14 の新機能を使用する。

- 安全でない C 関数（例: `memcpy`）の使用を完全に避け、
[このリスト](https://github.com/intel/safestringlib/wiki/SDL-List-of-Banned-Functions) に基づいて
安全な代替（例: `memcpy_s`）を使用する。

## メンテナンス

長期的なプロジェクトの安定性と保守性を確保するために、貢献中は以下のドメインに注意してください:

### ドキュメント

インターフェース（ヘッダーファイル）の protected および public エンティティはすべて
[Doxygen](https://www.doxygen.nl/index.html) に基づいてドキュメント化する必要があります。
[Doxygenize アクション](https://github.com/langroodi/doxygenize) が残りを処理し、
[GitHub Pages](https://langroodi.github.io/Adaptive-AUTOSAR/) にコードドキュメントを公開します。
コードが適切にドキュメント化されていない場合、
[アクションログ](https://github.com/langroodi/Adaptive-AUTOSAR/actions) から確認できます。

### ユニットテスト

各プロジェクトエンティティにユニットテストを作成することを強くお勧めします。
自動化されたワークフローが各コミット後にユニットテストを実行します。

### 外部依存関係

現在、プロジェクトは標準 C++14 とインハウスライブラリのみに依存しています。
プロジェクトにサードパーティの依存関係を追加したい場合は、最初に
[新しいディスカッション](https://github.com/langroodi/Adaptive-AUTOSAR/discussions/new) を作成してください。
ディスカッションはホイールの再発明を強制するためのものではなく、
セキュリティとポータビリティの問題（例: サプライチェーン攻撃）を避けるためのものです。

### コミットメッセージ（オプション: [Atom](https://github.com/atom/atom/blob/master/CONTRIBUTING.md#git-commit-messages) にインスパイア）

以下のカテゴリーに基づいて、適切な絵文字コードでコミットメッセージを開始してください:

| コミットカテゴリー | 絵文字コード | 絵文字アイコン |
| --------------- | ---------- | ---------- |
| 機能の追加・拡張 | `:beer:` | :beer: |
| ユニットテストの追加・拡張 | `:microscope:` | :microscope: |
| バグ修正 | `:syringe:` | :syringe: |
| コードのドキュメント化 | `:scroll:` | :scroll: |
| コードのリファクタリング | `:shower:` | :shower: |
| コードのクリーニング・削除 | `:toilet:` | :toilet: |
| パフォーマンスの向上 | `:rocket:` | :rocket: |
| ワークフローの改善・更新 | `:traffic_light:` | :traffic_light: |
| コンパイラーの最適化・警告修正 | `:construction:` | :construction: |
