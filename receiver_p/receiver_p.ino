#include <esp_now.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <AsyncTCP.h>


// Replace with your network credentials (STATION)
const char* ssid = "******"; //wifi name
const char* password = "*******"; //wifi password

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;
  bool present;
} struct_message;

struct_message incomingReadings;

JSONVar board;

AsyncWebServer server(80);  //default port for HTTP communication
AsyncEventSource events("/events");  //path to receive server-sent events

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  // Copies the sender mac address to a string
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  board["id"] = incomingReadings.id;
  board["present"] = incomingReadings.present;
  String jsonString = JSON.stringify(board);
  events.send(jsonString.c_str(), "new_readings", millis()); //send JSON string over SSE
  
  Serial.printf("Board ID %u: %u bytes\n", incomingReadings.id, len);
  Serial.printf("isPresent : %s \n", incomingReadings.present ? "true" : "false");
  Serial.println();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Present Absent Check</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin-top: 50px;
    }
    .seat {
      width: 100px;
      height: 100px;
      margin: 10px;
      display: inline-block;
      cursor: move;
      border: 1px solid #000;
      position: absolute;
      text-align: center;
      line-height: 100px;
    }
    .seat input {
      width: 90%;
      box-sizing: border-box;
      margin-top: 35px; /* Adjusted to position the input box correctly */
    }
    .occupied {
      background-color: green;
    }
    .vacant {
      background-color: red;
    }
    #container {
      position: relative;
      width: 100%;
      height: 80vh;
      border: 1px solid #ccc;
      margin: 0 auto;
    }
  </style>
</head>
<body>
  <h1>Present Absent Check</h1>
  <div id="container">
    <div id="seat-1" class="seat vacant" draggable="true">
      <input type="text" data-seat-id="1" placeholder="name1">
    </div>
    <div id="seat-2" class="seat vacant" draggable="true">
      <input type="text" data-seat-id="2" placeholder="name2">
    </div>
    <div id="seat-2" class="seat vacant" draggable="true">
      <input type="text" data-seat-id="2" placeholder="name2">
    </div>
  </div>
    
  <script>
    var source = new EventSource('/events');
    source.addEventListener('open', function(e) {
      console.log("Events Connected");
    }, false);
    source.addEventListener('error', function(e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    }, false);

  
  
    source.addEventListener('new_readings', function(e) {
      console.log("new_readings", e.data);
      var obj = JSON.parse(e.data);
      var seatElement = document.getElementById(`seat-${obj.id}`);
    
      if (seatElement) {
        seatElement.className = `seat ${obj.present ? 'occupied' : 'vacant'}`;
      }

  

      const nameInputs = document.querySelectorAll('input');
      nameInputs.forEach(input => {

      const seatId = input.getAttribute('data-seat-id');

      const savedName = localStorage.getItem(`seat-${seatId}-name`);
      if (savedName) {
        input.value = savedName;
      }
      
      input.addEventListener('change', (event) => {
        const newName = event.target.value;
        localStorage.setItem(`seat-${seatId}-name`, newName);
      });
      });

      const savedPosition = localStorage.getItem(`seat-${obj.id}-position`);
      if (savedPosition) {
        const position = JSON.parse(savedPosition);
        seatElement.style.left = `${position.left}px`;
        seatElement.style.top = `${position.top}px`;
      } else {
        seatElement.style.left = `${Math.random() * 300}px`;
        seatElement.style.top = `${Math.random() * 300}px`;
      }


      seatElement.addEventListener('dragstart', dragStart);

    },false);

    const container = document.getElementById('container');
    container.addEventListener('dragover', dragOver);
    container.addEventListener('drop', drop);


    function dragStart(event) {
      event.dataTransfer.setData('text/plain', event.target.id);
    }

    function dragOver(event) {
      event.preventDefault();
    }

    function drop(event) {
      event.preventDefault();
      const id = event.dataTransfer.getData('text');
      const seat = document.getElementById(id);
      const rect = container.getBoundingClientRect();
      const left = event.clientX - rect.left - seat.offsetWidth / 2;
      const top = event.clientY - rect.top - seat.offsetHeight / 2;
      seat.style.left = `${left}px`;
      seat.style.top = `${top}px`;

      const position = { left, top };
      localStorage.setItem(`${id}-position`, JSON.stringify(position));
    }

</script>
</body>
</html>
)rawliteral";

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

 server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}
 
void loop() {
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000; 
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }
}
