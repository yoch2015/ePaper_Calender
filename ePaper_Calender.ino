
/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *                  960 x 540
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "utilities.h"
#include "firasans.h"
#include "opensans24b.h"

#define COL_W 137
#define COL_H 70

uint8_t *framebuffer = NULL;
char buff[44];
char holiday[10];
uint16_t year, month, day;
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 9 * 3600;  // JST（日本標準時）
const int   daylightOffset_sec = 0;
const char* api_url = "https://holidays-jp.github.io/api/v1/date.json"; // 日本の祝日データAPI

int dayOfWeek(int year, int month, int day)
{
    if (month < 3)
    {
        year--;
        month += 12;
    }
    return (year + year / 4 - year / 100 + year / 400 + (13 * month + 8) / 5 + day) % 7;
}

int numberOfDaysInMonth(int year, int month)
{
    int numberOfDaysInMonthArray[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && year % 100 != 0 || year % 400 == 0)
    {
        numberOfDaysInMonthArray[1] = 29;
    }
    return numberOfDaysInMonthArray[month - 1];
}

void getJapaneseHolidays(const char* month) {
    HTTPClient http;
    http.begin(api_url);

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
        String payload = http.getString();
        Serial.println("祝日データ取得成功！");

        // JSONの解析
        DynamicJsonDocument doc(8192);
        deserializeJson(doc, payload);

        int numHolidays = 0;  // 祝日カウントをリセット

        for (JsonPair kv : doc.as<JsonObject>()) {
            String date = kv.key().c_str();
            String holidayName = kv.value().as<String>();

            // 指定された月の祝日をフィルタリング
            if (date.startsWith(month)) {
                Serial.print(date);
                Serial.print(" : ");
                Serial.println(holidayName);

                // 日付部分（"YYYY-MM-DD" の "DD"）を int に変換
                int day = date.substring(8, 10).toInt();

                // 配列に追加（配列がオーバーフローしないようチェック）
                if (numHolidays < 10) {  // 祝日が10個を超えないと仮定
                    holiday[numHolidays++] = day;
                }
            }
        }
    } else {
        Serial.print("HTTPリクエスト失敗: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Start Wifi connection
    WiFi.begin("---your-SSID---","---your-password---");
    // Wait until wifi connected
    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      i++;
      if (i > 120)
        break;
    }

    // NTPで現在時刻を取得
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("時間の取得に失敗しました");
        return;
    }
    char currentMonth[8];  // "YYYY-MM" (例: 2025-02)
    strftime(currentMonth, sizeof(currentMonth), "%Y-%m", &timeinfo);
    year = timeinfo.tm_year + 1900;  // `tm_year` は 1900年を基準とする
    month = timeinfo.tm_mon + 1;     // `tm_mon` は 0（1月）から始まるため +1

    Serial.print("今日の年月: ");
    Serial.print(year);
    Serial.print("年  ");
    Serial.print(month);
    Serial.println("月");

    // 日本の祝日を取得
    getJapaneseHolidays(currentMonth);
    WiFi.disconnect(true);

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) {
        Serial.println("alloc memory failed !!!");
        while (1);
    }

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) {
        Serial.println("alloc memory failed !!!");
        while (1);
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();
}

void loop()
{
    epd_poweron();

    char* weekdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    
    int32_t x0, y0, x1, y1, w, h;
    int32_t Colums, Lines;

    // week sun 0 - sat 6
    int firstDayOfWeek = dayOfWeek(year, month, 1);
    int numberOfDays = numberOfDaysInMonth(year, month);
    int numberOfRows = (firstDayOfWeek + numberOfDays - 1) / 7 + 1;

    FontProperties props = {
      .fg_color = 0,
      .bg_color = 15,
      .fallback_glyph = 0,
      .flags = 0
    };

    // Weekday Rect
    epd_fill_rect(0, 80, 137, EPD_HEIGHT, 224, framebuffer);
    epd_fill_rect(822, 80, 137, EPD_HEIGHT, 224, framebuffer);

    // Weekday Line
    epd_draw_hline(0, 80, EPD_WIDTH, 0, framebuffer);
    epd_draw_hline(0, 120, EPD_WIDTH, 0, framebuffer);
    for (int i = 190 ; i < 540 ; i += COL_H){
      epd_draw_hline(0, i, EPD_WIDTH, 0, framebuffer);
    }
    for (int i = COL_W ; i < 960 ; i += COL_W){
      epd_draw_vline(i, 80, EPD_HEIGHT, 0, framebuffer);
    }

    //Header Write
    x0 = 0; y0 = 0;
    sprintf(buff ,"%d / %02d", year, month);
    get_text_bounds((GFXfont *)&FiraSans, buff, &x0, &y0, &x1, &y1, &w, &h, &props);
    Colums = (EPD_WIDTH -w) /2;
    Lines = 40 + (h /2);
    writeln((GFXfont *)&OpenSans24B, buff, &Colums, &Lines, framebuffer);

    // Weekday Write
    for (int i = 0; i < 7; i++) {
      x0 = 0, y0 = 0;
      get_text_bounds((GFXfont *)&FiraSans, weekdays[i], &x0, &y0, &x1, &y1, &w, &h, &props);
      Colums = (137 * i) + ((137 - w) /2);
      Lines = 120 - ((40 - h) /2);
      if(i == 0 || i == 6){
        FontProperties props = {
          .fg_color = 0,
          .bg_color = 14,
          .fallback_glyph = 0,
          .flags = 0
        };
        write_mode((GFXfont *)&FiraSans, weekdays[i], &Colums, &Lines, framebuffer, WHITE_ON_BLACK,&props);
      } else {
        FontProperties props = {
          .fg_color = 0,
          .bg_color = 15,
          .fallback_glyph = 0,
          .flags = 0
        };
        write_mode((GFXfont *)&FiraSans, weekdays[i], &Colums, &Lines, framebuffer, WHITE_ON_BLACK,&props);
      }
    }
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(1000);

    //Day Write
    for (int i = 1; i <= numberOfDays; i++){
      sprintf(buff,"%d",i);
      int row = (firstDayOfWeek + i - 1) / 7;
      int column = (6 + firstDayOfWeek + i) % 7;
      int write_flag = 0;

      x0 = 0; y0 = 0;
      get_text_bounds((GFXfont *)&FiraSans, buff, &x0, &y0, &x1, &y1, &w, &h, &props);
      
      if(column == 0){
        Colums = (COL_W - w) /2;
      } else {
        Colums = (COL_W * column) + ((COL_W - w) /2);
      }

      if(row == 0){
        Lines = 190 - ((COL_H - h) /2);
      } else {
        Lines = (190 + (COL_H * row)) - ((COL_H - h) /2);
      }

      for(int j = 0 ; j < 10 ; ++j){
        if(holiday[j] == i){
          FontProperties props = {
           .fg_color = 0,
           .bg_color = 13,
           .fallback_glyph = 0,
           .flags = 0
          };

          if(column == 0){
            if(row == 0){
              epd_fill_rect(COL_W +1, 121 + COL_H , 136, 69, 208, framebuffer);
            } else {
              epd_fill_rect(COL_W +1, 121 + (COL_H * row), 136, 69, 208, framebuffer);
            }
          } else {
            epd_fill_rect((COL_W * column) +1, 121 + (COL_H * row), 136, 69, 208, framebuffer);
          }
          write_mode((GFXfont *)&FiraSans, buff, &Colums, &Lines, framebuffer, WHITE_ON_BLACK,&props);
          write_flag = 1;
        }
      }

      if(write_flag == 0){
        if(column == 0 || column == 6){
          FontProperties props = {
            .fg_color = 0,
            .bg_color = 14,
            .fallback_glyph = 0,
            .flags = 0
          };
          write_mode((GFXfont *)&FiraSans, buff, &Colums, &Lines, framebuffer, WHITE_ON_BLACK,&props);
        } else {
          FontProperties props = {
            .fg_color = 0,
            .bg_color = 15,
            .fallback_glyph = 0,
            .flags = 0
          };
          write_mode((GFXfont *)&FiraSans, buff, &Colums, &Lines, framebuffer, WHITE_ON_BLACK,&props);
        }
      }
    }

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    epd_poweroff();

    while(1);
}
