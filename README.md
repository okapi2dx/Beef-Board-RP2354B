# Beef Board RP2354B

RP2354B を搭載した Beef Board V2 の設計・ファームウェアと、Windows 用 Configurator のソースをまとめたプロジェクトです。基板データ、部品表、書き込み手順も公開しています。

## このプロジェクトでできること

- RP2354B 基板の回路図・基板レイアウト・部品表を確認する
- RP2354B 用ファームウェアを使い、BOOTSEL から UF2 を書き込む
- Configurator で RP2354B または AT90USB1286 の設定や入力状態を確認する

Configurator は、IIDX 系コントローラーの入力・LED・キー割り当てを設定する Windows アプリです。ボタンやターンテーブルの動作を画面で確認でき、基板に応じたファームウェア更新も行えます。

| 機能 | 内容 |
|---|---|
| 入力 | Joystick / Keyboard、ターンテーブルの方向・感度・デッドゾーン・入力時間・ディレイ、ボタンのデバウンス |
| LED | ボタン、ターンテーブル、センターバーの発光効果・色・明るさ・フェード・反転 |
| キー割り当て | キーボードのキー変更、物理ボタンの配置変更。B8も割り当て先に選べますが、実物のボタンやLEDはありません |
| モニター | ボタン、ターンテーブル、ファームウェア診断情報の表示 |
| ファームウェア | AT90USB1286 は HEX 書き込み、RP2354B は UF2 書き込みを案内 |

## 対応基板

| 基板 | ファームウェア | 書き込み方法 |
|---|---|---|
| RP2354B | [`Beef-Board-RP2354B.uf2`](firmware/Beef-Board-RP2354B.uf2) | BOOTSEL で ROM USB ドライブを開き、UF2 を手動でコピー |
| AT90USB1286 | [`Beef-Board-AT90USB1286.hex`](firmware/Beef-Board-AT90USB1286.hex) | Configurator から HEX を書き込み |

基板ごとに使える機能や書き込み方法が異なります。PHOENIXWAN UART を使ったターンテーブル／センターバー LED 制御は RP2354B 専用です。Configurator のポータブル EXE は配布・管理の都合でリポジトリに含めていません。Windows 版のビルド方法は [`configurator/README.md`](configurator/README.md) を参照してください。

## 開発とソースコードの由来

基板、ファームウェア、Configurator の設計・実装・デバッグ・文書作成に AI を活用しています。仕様と設計の採用判断はプロジェクト管理者が行います。実機で確認できている範囲は「状態」と各ファームウェアの README に記載しています。

Configurator は [HWXLR8/beef-board](https://github.com/HWXLR8/beef-board) を基にした Beef Board ソフトウェアのフォーク／派生コードを出発点として、RP2354B 対応を加えています。

## 内容

| 場所 | 内容 |
|---|---|
| `hardware/Beef-Board-RP2354B.kicad_pro` | KiCad 10 プロジェクト |
| `hardware/Beef-Board-RP2354B.kicad_pcb` | 指定された基板の配線整理版 |
| `hardware/Beef-Board-RP2354B.kicad_sch` | 基板に合わせた回路図 |
| `hardware/BEEF-BOARD.pretty/` | プロジェクト用フットプリント |
| `hardware/BOM-budget.csv` | 暫定部品表と概算価格 |
| `hardware/ROUTING.md` | 配線整理の変更量と残る確認事項 |
| `hardware/DRC.json` | 現在の KiCad DRC 結果 |
| `hardware/routing-changes.json` | 採用した配線変更の記録 |
| `firmware/Beef-Board-RP2354B.uf2` | BOOTSEL 書き込み用ファームウェア |
| `firmware/Beef-Board-AT90USB1286.hex` | AT90USB1286 用 HEX ファームウェア |
| `firmware/src/` | ファームウェアのソースコード |
| `configurator/` | RP2354B 対応 Configurator のソース一式（Web UI・Windows デスクトップ版） |

現在の PCB は今回指定されたファイルを基準に配線整理した版です。配線整理後、電源スイッチを J2 の3ピンヘッダに交換し、周辺の電源配線を変更しました。直近の編集では GND ビアを4本減らし、AE2 の5V入力配線を引き直しています。変更量は [`hardware/ROUTING.md`](hardware/ROUTING.md) を参照してください。

## 状態

- 現在の基板の KiCad DRC は未配線 0 件、指摘 46 件です。回路図との比較指摘は 67 件です。
- 回路図と基板の比較には USB-C 複合パッドの表現差などを含む指摘が残っています。実機評価および製造業者による確認は未実施です。
- BOM の価格・候補型番は暫定です。特に J4 の USB-C コネクタ、J3 端子台、一部コンデンサの仕様を発注前に確認してください。
- ファームウェアは RP2354B 向けにビルド済みですが、実機での USB・入出力・書き込み動作は未検証です。詳細は [firmware/README.md](firmware/README.md) を参照してください。

## 電源選択（J2）

基板表側の J2「POWER SELECT」に、2.54 mm ピッチの3ピンヘッダとショートピンを取り付けます。基板の印字「1・2・3」で番号を確認してください。

| 短絡するピン | 本体・ボタンLED | ターンテーブル・センターバー（VAUX） |
|---|---|---|
| 1–2 | J1 | J1 |
| 2–3 | J1 | J4 → ショットキーダイオード D1 |

J2 の1番は J1 の VBUS、2番は VAUX、3番は D1 のカソードです。D1 のアノードは J4 と J3 の外部電源入力に接続されています。ショートピンは一組だけ取り付けます。旧 JP1 は実装しません。ヘッダとショートピンの品番・電流定格は負荷に合わせて選定してください。

J3 の端子台は **1番が GND、2番が外部5V入力（EXT_5V）** です。接続前に基板のピン番号を確認してください。

## 書き込み

J61（BOOTSEL_SW–GND）を短絡して J60（RUN–GND）でリセットするか、接続済みファームウェアから BOOTSEL コマンドを送ります。J1 の USB 接続で ROM の USB ドライブが表示されたら `firmware/Beef-Board-RP2354B.uf2` をコピーしてください。J61 の短絡は USB ドライブが現れたら外します。

Configurator は [`configurator/README.md`](configurator/README.md) に従ってソースからビルドできます。RP2354B 用のファームウェア選択と BOOTSEL 案内があり、旧 AT90USB1286 も引き続き扱えます。STM32 用の書き込み経路は含みません。
