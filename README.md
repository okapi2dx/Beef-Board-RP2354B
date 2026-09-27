# Beef Board RP2354B

Beef Board V2 の基板設計、ファームウェア、Windows 用 Configurator をまとめたプロジェクトです。Configurator は、RP2354B と従来の AT90USB1286 の2種類のマイコンを搭載した基板に対応します。

## Configurator

Beef Board Configurator は、IIDX 系コントローラーの設定・調整・診断を行う Windows アプリです。基板を USB 接続し、入力方式やターンテーブル、LED、キー割り当てを設定できます。コントローラーモニターでボタンやターンテーブルの入力状態も確認できます。

主な機能:

- 入力方式（Joystick / Keyboard）、ターンテーブルの感度・方向・デッドゾーン・入力時間の調整
- ボタン、ターンテーブル、センターバーのLED効果・色・明るさの設定
- キーボード割り当てと物理ボタン配置の変更。B8も割り当て先に選べますが、基板にB8ボタンやLEDはありません
- ボタン、ターンテーブル、ファームウェア診断情報のモニター
- 基板に応じたファームウェア書き込み

## 対応基板

このプロジェクトは、異なる2種類のマイコンを搭載した基板を扱います。

| マイコン／基板 | 説明 | ファームウェア／書き込み |
|---|---|---|
| **RP2354B** | このリポジトリの基板設計とファームウェア。11個のボタン入力、ターンテーブル用エンコーダー、ボタンLED、RGB LED出力に対応 | [UF2](firmware/Beef-Board-RP2354B.uf2)をBOOTSELのUSBドライブへ手動でコピー |
| **AT90USB1286** | 従来のBeef Board基板。Configuratorから設定・診断できるレガシー対応 | [HEX](firmware/Beef-Board-AT90USB1286.hex)をConfiguratorから書き込み。ソースコードはこのリポジトリに含みません |

基板により、使用できる入出力や書き込み方法が異なります。Windows版Configuratorのビルド方法は [`configurator/README.md`](configurator/README.md) を参照してください。ポータブルEXEはリポジトリに含めていません。

## PHOENIXWAN UART

RP2354B基板のUART端子は、**従来型PHOENIXWANのターンテーブルリングとセンターバーLEDを接続して制御する用途**です。ターンテーブルリングはJ42、センターバーはJ43に接続し、ConfiguratorのLED設定に合わせて制御します。このUART機能はAT90USB1286基板にはありません。

UARTプロトコルで設定できる色や発光パターンには制限があります。対応する制御内容と接続上の注意は [`firmware/README.md`](firmware/README.md) を参照してください。実機でのPHOENIXWAN接続確認は未実施です。

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
