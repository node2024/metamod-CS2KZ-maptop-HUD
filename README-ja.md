# KZ MapTop HUD 0.1.13

CS2KZ **v0.0.189** 向けの独立した Linux x86_64 Metamod プラグインです。
`!maptop` で現在のマップのタイムランキングをブロック表示します。
記録ごとに横長の情報ボタンとReplayを並べ、情報ボタンでその走行記録の詳細、Replayで即再生します。
メニューの描画と入力には [mm-cs2menus](https://github.com/FemboyKZ/mm-cs2menus) を使います。

**現行Workshopアドオンで使える表示改善版です。** 0.1.1までのチャット解析クラッシュ対策を維持しています。
Linux バイナリのビルド、Metamod API18 の実バイナリ検査、
ローカルランキング SQL のテスト、公開 API の応答解析を検証しました。
CS2 サーバーでの読み込み、HUD の見た目・マウス操作、リプレイ再生は検証が必要です。
既存の RTV HUD や CS2KZ のファイルは変更しません。

## 0.1.13 の操作・表示

**標準のCS2Menusだけを使用し、サーバー側・Workshop側とも拡張しません。**
ZIPに含める実行バイナリはMapTop HUDの `.so` だけです。

Localの記録詳細にも、同じプレイヤー・マップ・コース・モード・Overall / Proの **Global PB順位** を追加しました。
Rankの表示例は `Local Pro #5 / 11` と `Global PB Pro #123` です。
Global側のPBが選択したLocalタイムと違う場合は、そのPBのタイム・TP数も併記します。
他のタイムに付いた順位を、選択したLocalタイム自身の順位として扱いません。
順位の取得先は既存cfgの `apiUrl` です。詳細を開いた時だけ1リクエストを送り、Localの表示を先に出します。
記録なし・順位なしは `No ranked record`、通信失敗・API無効時は `Unavailable` と表示します。
Global一覧から開いた詳細は取得済みの順位を使うため、追加通信を行いません。
プレイヤー指定APIの `total: 1` はランキング人数ではないため、Global PBの分母として表示しません。
詳細を閉じる、別の記録を開く、フィルターを変更するなどした後に届く応答では、画面を再表示しません。
Actionsへ移動している場合は、そちらのGlobal PB欄を更新します。

0.1.11の詳細本文は、CS2Menusの `StripColors` が通常の改行（LF）を消すため、ゲーム内で項目が連結していました。
0.1.13はポップアップ専用の本文を作り、除去されないUnicode行区切り（U+2028）を使います。
見出しはプレイヤー名、本文はタイム・TP数、順位、モード・取得先、MAP / COURSEの順です。
SERVER・RECORDED・STATSは実際の値がある区画だけを表示し、リプレイの有無を末尾に置きます。
未取得サーバー名やSync等の取得不可の説明はActions画面に残し、ポップアップ本文では省略します。
標準の文字除去を再現したテストで区切りの保持を確認しました。ゲーム内での改行描画は未確認です。
本文のフォント・色・枠の幅は既存アドオンの固定スタイルです。HTMLタグやCSSの埋め込みは行いません。

既存の **Studio** レイアウトを使い、1ページ5件を表示します。
各記録は情報ボタンを2枠、Replayを1枠として横に並べます。独立したProfileボタンはありません。

| 順位・プレイヤー情報（2枠・クリックで詳細） | 再生（1枠） |
|---|---|
| #1 Player Name / 下段: 05:14.453 \| TP 0 | Replay ▶ |

標準の `SetItemSpan(..., 2)` で3列のうち2枠を結合します。CS2Menus側の拡張は不要です。
横長ボタンは標準の2段・左寄せスタイルで表示します。
情報ボタンの長い名前は省略し、詳細ではより長い名前を表示します。
タイムは小さなテーブルセルへ分割せず、1つのサブテキストに保ちます。
Courses＋5件×2項目＋ページャーで、標準Studioの18項目・描画24スロットの上限に収めています。

- 上部を **Source（Local / Global）・Mode（VNL / CKZ）・Type（Overall / Pro）** の3つのドロップダウンに統一。
  内部のAPI値は `vanilla` / `classic` です。切り替えるとページを先頭へ戻し、他の選択を維持します。
  選択中の項目を再度選んでも、必須フィルターが空にならないようにします。
- **Courses** をランキングの最上段（#1の上）に固定しました。下部のFilters行はありません。
  コース名を大きい主表示（標準16px・太字）にし、下段の小さい補足表示（13px）を `Courses ▾` にしました。
  現行アドオンのボタン高さは固定のため、そのままです。
  押した先にはコース名だけを並べ、Source・Mode・Typeなどの項目や戻る行を入れません。
  別のコースを選ぶとページ1から取得し直し、現在と同じコースなら元のページへ戻ります。
  選択せず終了するときは標準の閉じる操作を使います。
- Source・Mode・Typeは上部ドロップダウンから選びます。追加のカテゴリボタンは設けません。
  ドロップダウンの色は既存アドオンの標準配色です。
- **背景クリックで半透明・操作不能になる挙動を補正**します。
  Studioは背景クリックで視点操作へ切り替わるため、そのイベントの処理後にMapTop自身の画面だけを再表示します。
  ページ・Source・Mode・Type・コースと取得済みの記録を保ち、読み込み中のリクエストも継続します。
  エラー表示中はそのメッセージを保ちます。コース選択画面や別プラグインの画面、閉じたメニューは再表示しません。
- **横長の情報ボタンをクリックすると、そのタイムの詳細ダイアログ**を表示します。
  名前を見出しにし、Runtime（TP数）/ Rank（Overall・Pro、順位と人数）/ Mode / Source / Map / Course をまとめます。
  GlobalではAPIが返す実際のサーバー名とポイントを表示します。
  Localでは記録日時と、DBのMetadataに保存されたSplits・Checkpoints・Stagesの通過数と最後の通過タイムを表示します。
  ローカルDBには走行元サーバー名が保存されないため、その区画はポップアップでは省略し、Actions画面で `Not stored in local DB` と表示します。
  SteamIDやRun IDの列挙を避け、リプレイのない記録でも詳細を開けます。
  Localでは開いた後にGlobal PB順位を取得し、Globalでは取得済みの情報だけで表示します。
- 詳細の **Actions** から **Replay** と **Show CS2KZ URL (console)** を選べます。
  一覧の **Replay ▶** は、詳細を開かずに直接再生します。どちらも再生確認を挟みません。
  再生できない記録ではReplayを無効にします。
  ファイルが後から消えた場合も再生を止め、一覧に戻します。
- **Show CS2KZ URL (console)** は、公式サイトの該当プレイヤーURLをコンソールに出力します。
  `https://cs2kz.org/profile/<SteamID64>` をコピーしてブラウザーへ貼り付けてください。
- **一番下の `< Page n >`** は、一覧と同じ通常ボタン3個で表示します。
  標準Choiceの分割表示をやめ、138px幅・3px余白の共通グリッドに統一しました。
  左右の矢印を直接押して前後へ移動します。移動先がない側は無効になり、押しても移動しません。
  中央は現在ページの表示で、押してもページを変更しません。

テーブルの行末「…」は使用せず、標準のブロックボタンへ独立した操作を割り当てます。
**Show CS2KZ URLはブラウザー起動やクリップボードへの自動コピーを行いません。**

画像にあるPerfect Bhops・Air Strafes・Air Time・Sync・Overlap・Dead Air・Bad Angles・Scroll Efficiencyは、
[公開APIの記録スキーマ](https://github.com/KZGlobalTeam/cs2kz-website/blob/master/openapi-types.ts) と
[v0.0.189の保存Metadata](https://github.com/KZGlobalTeam/cs2kz-metamod/blob/da535d95714b40de625134973a6effe5c20325e4/src/kz/timer/kz_timer.cpp#L1647)
には含まれません。Actions画面で `Sync / Air Time: unavailable` と表示し、別のジャンプ統計や現在のプレイヤーの値を代用しません。
Metadataが空・旧形式・不正な場合は区間統計だけを省略し、ランキングや詳細の表示を継続します。
標準ダイアログ内はラベル付きの縦並びで、画像のWebサイトと同じ複数列の装飾は行いません。

上部の戻る・進む・更新・折りたたみアイコンは、標準CS2Menusのレイアウトに常設されています。
公開APIにはこれらを非表示にする設定がないため、閉じるだけにする変更は含みません。
履歴や更新のないときも、対応するアイコンは非表示ではなく無効表示になります。

**0.1.13のゲーム内描画・マウス操作は未確認**です。コード上の構成・操作先と実バイナリのAPIをテストしています。

更新時はサーバーを停止し、`kz_maptop_hud.so` を差し替えて再起動してください。
`meta list` の **KZ MapTop HUD 0.1.13** を確認します。既存cfg・DB・Workshopの更新は不要です。
0.1.5同梱のCS2Menus拡張版を導入していた場合は、バックアップした標準の `cs2menus.so` に戻してください。

## 0.1.1 の修正・0.1.0 からの更新

0.1.0 はSDK側の `g_pCVar` の初期化が漏れており、`!mtop` などのチャット解析で
`CCommand::Tokenize()` が null ポインターを参照してクラッシュする問題がありました。
0.1.1 は `ICvar` 取得直後、フック登録前にSDK側へ設定します。
ランキング・リプレイ・プロフィールの処理や既存cfgの参照方法はそのままです。

更新時はサーバーを停止し、`game/csgo/addons/kz_maptop_hud/bin/linuxsteamrt64/kz_maptop_hud.so`
を最新の0.1.13へ差し替えて再起動してください。既存cfgやDBの変更は不要です。
`meta list` の **KZ MapTop HUD 0.1.13** を確認し、`!mtop` で再確認してください。
`kz_reload_config` はバイナリの差し替えには使用できません。
修正版の実サーバーでの動作は、まだ確認していません。

## 機能

- 現在のマップ・プレイヤーのモードとコースを初期選択。コース外なら取得先の先頭コースを選択。
- **Local / Global**、**Overall / PRO（0 TP）**、**CKZ / VNL**、コースを切り替え。
- 横長のタイム・順位・TP・名前とReplayを5件ずつ表示。下部の左右ボタンで前後へ移動。
- 横長の情報ボタンで走行記録の詳細ダイアログ。プレイヤー全体の評価ではなく選択した記録を表示。
- ダイアログを使用できない場合は、記録詳細のActions画面を直接表示。
- 自分の情報ブロックを標準の緑色で強調。
- プレイヤー名は通常のテキストとして渡し、HTMLとして評価しない。
- マップ変更・切断・画面終了・表示更新後に届いた古い応答を破棄。

プロフィールのWeb URLは文字表示とコンソール出力です。Steam Overlayや外部ブラウザーを自動で開く処理は含みません。
リプレイの一時停止・速度変更などは、CS2KZ 標準の再生コントロールを使用してください。

## 必要なもの

- Linux x86_64 CS2 サーバー。
- **Metamod API18 / KHook**。API17版では読み込めません。
- **CS2KZ v0.0.189**（公開インターフェース `ICS2KZ002`）。
- **標準のCS2Menus**（公開インターフェース `ICS2Menus008`）。サーバー側の改造や差し替えは不要です。
- マウス操作には MultiAddonManager と CS2Menus の Workshop アドオン **3801628903**。
- ローカル記録には、CS2KZ と互換性のある **SQLMM** と既存の CS2KZ DB。
- グローバル記録には、CS2KZ の `apiUrl` への HTTP(S) 通信と、サーバーの Steam HTTP 機能。

CS2Menus の `cfg/cs2menus/core.cfg` で `Panorama` の `MountAddon` が `1` なら、
CS2Menus が MultiAddonManager 経由で自身のアドオンをマウントします。
既存の CS2KZ / RTV HUD などのアドオンIDは保持してください。
Panorama が利用できない場合は CS2Menus の HTML、さらにチャットへフォールバックします。
HTML は W/S で移動、E で選択。マウス操作には Panorama の準備が必要です。
上部ドロップダウンと詳細ダイアログはPanoramaのみです。
HTML・チャットでは各ブロックを通常の行として表示します。Courses行でコースを選び、
Replay行で即再生、名前・タイムの行で詳細を表示します。下部の左右の行でページを切り替えます。
Source・Mode・Typeの上部ドロップダウンによる変更はPanorama向けです。

## 導入

1. 検証用サーバーを停止し、ZIP内の **`game/csgo/` の中身**をサーバーの `game/csgo/` に配置します。
2. 接続情報の追加設定は不要です。**`cfg/cs2kz-server-config.txt` をそのまま参照**します。
   CS2KZ と同じ Valve KeyValues パーサーで `db` と `apiUrl` を読み取ります。
3. `addons/kz_maptop_hud/config.json` は表示時間 `menu_seconds` のみです。省略時は120秒です。
4. CS2Menus、CS2KZ、SQLMM、MultiAddonManager がある状態で起動し、サーバーコンソールの `meta list` で
   **KZ MapTop HUD 0.1.13** を確認します。
5. ゲーム内で `!mtop` を実行し、Local / Global の両方を確認します。

参照する設定と解釈は次のとおりです。

| CS2KZ cfg の項目 | 用途 |
|---|---|
| `db / driver` | `sqlite` または `mysql`。`db` がなければローカル記録は無効 |
| `db / database` | SQLiteではCS2KZと同じ `addons/cs2kz/data/<値>.sqlite3` に変換。MySQLではDB名 |
| `db / host`・`user`・`pass` | MySQL接続先と認証情報 |
| `db / port`・`timeout` | MySQL設定。省略時はCS2KZと同じ3306・60秒 |
| `apiUrl` | グローバル記録の取得先。省略時はCS2KZと同じ既定URL、空なら無効 |

`apiKey` はこのプラグインでは使用しません。リプレイのダウンロードはCS2KZに任せるため、
`replayDownloadUrl` もCS2KZ側の設定が使われます。接続情報を別のJSONへ複製しません。
コメント、引用符、入れ子、インクルードなども、CS2KZと同じエンジンのパーサーが処理します。

SQLiteでは参照先ファイルが存在しなければ接続しません。誤ったパスに空DBを作ることを防ぎます。
DBには SELECT のみ発行し、CS2KZ のマイグレーションや記録更新は行いません。
API URL と表示時間の変更は、サーバーコンソールの **`kz_reload_config`** で再読み込みします。
DB接続情報の変更は、CS2KZ本体と同じく **サーバー再起動後に反映**します。
不正なcfgの再読み込み時は動作中の設定を保持します。
ローカルDBの接続失敗時は5秒間隔で再接続を試みます。
既存cfgにあるDBのパスワードなどをHUDへ表示する処理はありません。

## コマンドと操作

| 入力 | 動作 |
|---|---|
| `!maptop` / `/maptop` | 標準コマンドの実行後にHUDを開く |
| `!mtop` / `/mtop` | HUDを直接開く |
| `!maptop_hud` / `/maptop_hud` | HUDを直接開く |
| クライアントコンソール `kz_maptop_hud` | HUDを直接開く |
| クライアントコンソール `kz_maptop`（引数なし） | 標準コマンドの実行後にHUDを開く |

`!maptop` の標準チャット／コンソール出力はそのまま残ります。
同じコマンドを扱う別プラグインのKHookを強制的に停止する処理は使っていません。
HUDだけを使う場合は `!mtop` を使用してください。
引数付き `!maptop` はCS2KZ標準の処理に任せます。この初版は現在のマップを対象にします。

一覧の最上段の **Courses** からコースだけを選択します。
Panoramaでは上部の **Source / Mode / Type** で取得先・モード・カテゴリを切り替えられます。
Replayの **Unavailable** は、ローカルのリプレイファイルがない場合、またはグローバル API がリプレイ非公開と返した場合です。
記録が残っていても、保存上限や削除によりリプレイがないことがあります。

再生確認は表示しません。CS2KZのリプレイボットは共有なので、他の観戦者の再生にも影響し、
自分の走行が中断される場合があります。一覧または記録詳細内のReplayから、呼び出した本人のスロットで
`kz_replay <検証済みUUID>` をサーバー側から実行します。任意の文字列をコマンドへ渡しません。

## ビルド

Linux の Git、curl、CMake、C++20 対応コンパイラ、Python 3 が必要です。
`bootstrap.sh` は固定リビジョンの SDK / Metamod / 公開APIヘッダー / JSON を `deps/` に取得します。

```bash
bash bootstrap.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 tools/package.py build/kz_maptop_hud.so
```

成果物は `build/kz_maptop_hud.so`、配置用ZIPは `release/KZ-MapTop-HUD-0.1.13-linux.zip`。
ZIPには導入用の `.so`・VDF・初期設定、説明書・ライセンス、対応するソースと再ビルド用ファイル、
検証用のマニフェスト・SHA-256一覧を含めます。梱包するファイルの種類と配置ファイルを
`tools/package.py` で限定し、ビルド中間物・調査データ・ログ・バックアップの混入を防ぎます。
`release/` には最新版のZIPだけを残します。
CS2と同じホストでコンパイルするときは、サーバーを停止してメモリ消費に注意してください。
配布バイナリは Steam Runtime Sniper SDK 内の GCC 10.3 でビルドしています。
Windows ネイティブ用DLLは含みません。Windowsでは Docker / WSL 内で Linux 用にビルドしてください。

SDKなしでロジックテストだけを行う場合も、`deps/json/nlohmann/json.hpp` は必要です。

```bash
cmake -S . -B build-tests -DMAPTOP_BUILD_PLUGIN=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## 検証記録と残る確認

2026-10-24 作成。

- `core`：SteamID変換、UUID検証、UTF-8切り詰め、タイム丸め、REST応答・PRO条件、古いリクエストの識別。
  Global PBのOverall / Pro順位、別タイムの明示、分母の誤表示防止、プレイヤー・マップ・コース・モード・styleの照合、順位なしも確認。
  記録詳細のサーバー名・順位人数・ポイント・任意項目の欠落、区間Metadataの通過タイム・未通過値・不正値も確認。
  連続したタイム表示、分のゼロ埋め、時間単位の記録、5件＋先読み1件のページ境界も確認。
  背景クリックのprotobuf判定、フィールド順・未知フィールド、壊れた長さ・整数オーバーフロー・対象外イベントの除外も確認。
- `local_ranking`：実際に生成するSQLをSQLiteで実行。PRO、同タイム、重複PB、BAN・style・別モード・別コースの除外、ページ境界。
  同じ絞り込みでのランキング人数と、選択した記録に対応するMetadata・日時の取得も確認。
- `binary_api18`：実際の `.so` をロードし `CreateInterface("ISmmPlugin")` と `GetApiVersion() == 18` を検査。
- `cs2kz_config`：Valve KeyValuesでCS2KZ形式のcfgを解析し、SQLite名からの実パス、MySQLの認証項目・省略値、独自API URL・API無効化を確認。
- `load_cvar_tokenize`：本体のLoadとチャット解析を、実際のSDKトークナイザと最小限のインターフェーススタブで実行。SDK側g_pCVarの初期化、チャット・リプレイのトークン解析、遅延ロードを確認。
- `board_controls_actions`：本体の画面生成・コールバックを公開APIのスタブで実行。
  Studio背景クリック後の再表示、ページ・フィルター・読み込み中リクエスト・エラー表示の保持、
  別プラグインの画面・List画面・閉じたメニュー・不正な入力で再表示しないことも確認。
  Coursesの先頭配置・コースだけの選択画面・同じコースへの復帰・変更時のページ初期化・古い取得先のコース無効化、
  Studioの情報2枠＋Replay1枠、標準ページ上限、3ドロップダウンと必須値の復元、詳細ダイアログ、URL出力、
  CS2Menusが通常の改行を除去した後もポップアップ用の区切りと内容が保持されること、値のない区画を省略すること、
  Global順位の非同期反映・ポップアップ差し替え時のキャンセル通知・Actions画面への反映、
  閉じた詳細・別の記録・プレイヤー交代・フィルター変更後に届いた順位で画面を変更しないこと、
  Replayボタンからの即再生、UUID・呼出元スロット、再生不可・ファイル消失、通常ボタン3個のページャーと移動端の拒否、
  空の一覧、読み込み中の切り替え、古い応答の破棄を確認。
  実際のPanorama描画を検査するテストではありません。
- 同じ回帰テストを配布済み0.1.0のソースで実行し、SDK側g_pCVarの検査で失敗することを確認。0.1.1では5テストすべて成功。
- 公開APIで `kz_grotto / Main / Classic` の Overall / PRO 各11件を取得し、同じC++パーサーで解析。
- 公開APIの `kz_hex / main / classic` で、プレイヤー指定のOverall / Pro PB応答を解析。
  `total: 1` に対して実際のGlobal順位が1位・2位の応答を確認し、人数と順位を混同しないことを検証。

実サーバーで必要な確認：プラグインのロード順、Workshopマウント、表の見た目、クリック、
MySQL接続、HTMLフォールバック、リプレイ再生、RTV投票との同時利用、切断・マップ変更・依存プラグイン再読み込み。
この段階では「ゲーム内動作確認済み」とはしていません。

## ライセンス・由来

MapTop HUDはAGPL-3.0。対応するソース、参照する公開ヘッダーとライセンスをZIPに含めます。
詳細と固定リビジョンは `LICENSE` / `NOTICE.md` を参照してください。
