void setup() {
  Serial1.setRX(17);  // RXピンの設定
  Serial1.begin(9600);  // シリアル通信の開始
}

void loop() {
  static String gpsBuffer = "";  // GPSデータを一時的に保持するバッファ

  while (Serial1.available()) {
    char c = Serial1.read();
    gpsBuffer += c;  // 文字をバッファに追加

    // 行の終わり（改行）を検出
    if (c == '\n') {
      // バッファに$GPRMCが含まれているか確認
      if (gpsBuffer.startsWith("$GPRMC")) {
        Serial.println(gpsBuffer);  // RMCフォーマットのデータを出力
      }

      gpsBuffer = "";  // バッファをクリア
    }
  }
}
