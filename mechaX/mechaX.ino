#include <Arduino.h>
#include "WiFi.h"
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <iostream>
#include <sstream>
#include <ESP32Servo.h>

struct ServoPins
{
  Servo servo;
  int servoPin;
  String servoName;
  int initialPosition;  
};
std::vector<ServoPins> servoPins = 
{
  { Servo(), 27 , "base", 90},
  { Servo(), 26 , "shoulder", 90},
  { Servo(), 25 , "elbow", 90},
  { Servo(), 33 , "gripper", 90},
};

struct RecordedStep
{
  int servoIndex;
  int value;
  int delayInStep;
};
std::vector<RecordedStep> recordedSteps;

bool recordSteps = false;
bool playRecordedSteps = false;

unsigned long previousTimeInMilli = millis();

const char* ssid = "wifi";
const char* password = "password";

AsyncWebServer server(80);
AsyncWebSocket serverInput("/Input");

const char* homePage = R"HTMLHOMEPAGE(
 <!DOCTYPE html>
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            .slidecontainer {
            width: 100%;
            }

            .slider {
            width: 100%;
            height: 25px;
            background: #d3d3d3;
            outline: none;
            opacity: 0.7;
            -webkit-transition: .2s;
            transition: opacity .2s;
            }

            .slider:hover {
            opacity: 1;
            }

            .title{
                font-size: 2em;
                font-weight: bold;
                color: white;
                margin: 10px;
                text-align: center;
            }
            .card {
            box-shadow: 0 4px 8px 0 rgba(0, 0, 0, 0.2);
            transition: 0.3s;
            width: 50%;
            border-radius: 5px;
            background-color: white;
            padding: 20px;
            margin: 10px;
            }
            .card-container {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            flex-wrap: wrap;
            }
            .title-value {
            display: flex;
            flex-direction: row;
            justify-content: space-between;
            align-items: center;
            }
            .value{
                font-size: 2em;
                font-weight: bold;
                color: #cccccc;
            }

            .record-button {
            background-color: #008000;
            border: none;
            color: white;
            padding: 15px 32px;
            border-radius: 20px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 4px 2px;
            cursor: pointer;
            }

            .record-button:hover {
                background-color: #4da64d;
            }

            .play-button {
            background-color: #3824b0;
            border: none;
            color: white;
            padding: 15px 32px;
            border-radius: 20px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 4px 2px;
            cursor: pointer;
            }

            .play-button:hover {
                background-color: #7466c8;
            }

            .nav-bar {
            overflow: hidden;
            background-color: #3824b0;
            position: fixed;
            top: 0;
            width: 100%;
            }

            /* for medium and small screens make card 100% width */
            @media screen and (max-width: 768px) {
            .card {
                width: 80%;
            }
            }
        </style>
    </head>
    <body>
        <div class="card-container">
            <div class="nav-bar">
                <h1 class="title">MechaX Control</h1>
            </div>
            <div class="card" style="margin-top: 4rem;">
                <div class="title-value">
                <h2>Gripper</h2>
                <p id="gripper-val" class="value">90</p>
                </div>
            <input type="range" min="0" max="180" value="90" class="slider" id="gripper" oninput='sendSliderOutput("gripper",value)'>

            <div class="title-value">
                <h2>Elbow</h2>
                <p id="elbow-val" class="value">90</p>
                </div>
            <input type="range" min="0" max="180" value="90" class="slider" id="elbow" oninput='sendSliderOutput("elbow",value)'>

            <div class="title-value">
                <h2>Shoulder</h2>
                <p id="shoulder-val" class="value">90</p>
                </div>
            <input type="range" min="0" max="180" value="90" class="slider" id="shoulder" oninput='sendSliderOutput("shoulder",value)'>

            <div class="title-value">
                <h2>Base</h2>
                <p id="base-val" class="value">90</p>
                </div>
            <input type="range" min="0" max="180" value="90" class="slider" id="base" oninput='sendSliderOutput("base",value)'>
            </div>

            <div class="card">
                <div class="title-value">
                    <input type="button" class="record-button" id="record" value="Record" onclick='onclickRecord(this)'>
                    <input type="button" class="play-button" id="play" value="Play" onclick='onclickPlay(this)'>
                </div>
            </div>
        </div>
        <script>
            var serverInputUrl = "ws:\/\/" + window.location.hostname + "/Input"; 
            var serverInput;
            
            function initServerWebSocket() 
            {
                serverInput = new WebSocket(serverInputUrl);
                serverInput.onopen    = function(event){};
                serverInput.onclose   = function(event){setTimeout(initServerWebSocket, 2000);};
                serverInput.onmessage = function(event){
                    var data = event.data.split(",");
                    var slider = document.getElementById(data[0]);
                    slider.value = data[1];
                    var sliderValue = document.getElementById(data[0] + "-val");
                    sliderValue.innerHTML = data[1];
                    if(data[0] == "record")
                    {
                        var button = document.getElementById("record");
                        button.value = data[1] == "1" ? "Stop" : "Record" ;
                        button.style.backgroundColor = button.value == "Stop" ? "#ff0000" : " #008000";
                        disableComponents(button, button.id);
                    }
                    else if(data[0] == "play")
                    {
                        var button = document.getElementById("play");
                        button.value = data[1] == "1" ? "Stop" : "Play" ;
                        button.style.backgroundColor = button.value == "Stop" ? "#ff0000" : " #008000";
                        disableComponents(button, button.id);
                    }
                };
            }

            function sendSliderOutput(key, value) 
            {
            var data = key + "," + value;
            var output = document.getElementById(key + "-val");
            output.innerHTML = value;
            serverInput.send(data);
            }

            function onclickRecord(button) 
            {
            button.value = button.value == "Record" ? "Stop" : "Record" ;
            button.style.backgroundColor = button.value == "Stop" ? "#ff0000" : " #008000";
            var value = button.value == "Stop" ? "1" : "0";
            var data = button.id + "," + value;
            disableComponents(button, button.id);
            serverInput.send(data);
            }
            function onclickPlay(button) 
            {
            button.value = button.value == "Play" ? "Stop" : "Play" ;
            button.style.backgroundColor = button.value == "Stop" ? "#ff0000" : "#3824b0";
            var value = button.value == "Stop" ? "1" : "0";
            var data = button.id + "," + value;
            disableComponents(button, button.id);
            serverInput.send(data);
            }
            function disableComponents(button, id) 
            {
                if (button.value == "Stop") 
                {
                    if(id != "record")
                    {
                        document.getElementById("gripper").disabled = true;
                        document.getElementById("elbow").disabled = true;
                        document.getElementById("shoulder").disabled = true;
                        document.getElementById("base").disabled = true;
                        document.getElementById("record").disabled = true;
                    }
                    else
                    {
                        document.getElementById("play").disabled = true;
                    }
                }
                else 
                {
                    document.getElementById("gripper").disabled = false;
                    document.getElementById("elbow").disabled = false;
                    document.getElementById("shoulder").disabled = false;
                    document.getElementById("base").disabled = false;
                    id == "record" ? document.getElementById("play").disabled = false : document.getElementById("record").disabled = false;
                }
            }
            window.onload = initServerWebSocket;
        </script>
    </body>
</html>
)HTMLHOMEPAGE";

void handleRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", homePage);
}

void onServerInputWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendCurrentRobotArmState();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        // Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str()); 
        int valueInt = atoi(value.c_str()); 

        if (key == "record")
        {
          recordSteps = valueInt;
          if (recordSteps)
          {
            recordedSteps.clear();
            previousTimeInMilli = millis();
          }
        }  
        else if (key == "play")
        {
          playRecordedSteps = valueInt;
        }
        else if (key == "base")
        {
          writeServoValues(0, valueInt);            
        } 
        else if (key == "shoulder")
        {
          writeServoValues(1, valueInt);           
        } 
        else if (key == "elbow")
        {
          writeServoValues(2, valueInt);           
        }         
        else if (key == "gripper")
        {
          writeServoValues(3, valueInt);     
        }   

      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void sendCurrentRobotArmState()
{
  for (int i = 0; i < servoPins.size(); i++)
  {
    serverInput.textAll(servoPins[i].servoName + "," + servoPins[i].servo.read());
  }
  serverInput.textAll(String("record,") + (recordSteps ? "Stop" : "Record"));
  serverInput.textAll(String("play,") + (playRecordedSteps ? "Stop" : "Play"));  
}

void writeServoValues(int servoIndex, int value)
{
  if (recordSteps)
  {
    RecordedStep recordedStep;       
    if (recordedSteps.size() == 0) // We will first record initial position of all servos. 
    {
      for (int i = 0; i < servoPins.size(); i++)
      {
        recordedStep.servoIndex = i; 
        recordedStep.value = servoPins[i].servo.read(); 
        recordedStep.delayInStep = 0;
        recordedSteps.push_back(recordedStep);         
      }      
    }
    unsigned long currentTime = millis();
    recordedStep.servoIndex = servoIndex; 
    recordedStep.value = value; 
    recordedStep.delayInStep = currentTime - previousTimeInMilli;
    recordedSteps.push_back(recordedStep);  
    previousTimeInMilli = currentTime;         
  }
  servoPins[servoIndex].servo.write(value);   
}

void playRecordedRobotArmSteps()
{
  if (recordedSteps.size() == 0)
  {
    return;
  }
  //This is to move servo to initial position slowly. First 4 steps are initial position    
  for (int i = 0; i < 4 && playRecordedSteps; i++)
  {
    RecordedStep &recordedStep = recordedSteps[i];
    int currentServoPosition = servoPins[recordedStep.servoIndex].servo.read();
    while (currentServoPosition != recordedStep.value && playRecordedSteps)  
    {
      currentServoPosition = (currentServoPosition > recordedStep.value ? currentServoPosition - 1 : currentServoPosition + 1); 
      servoPins[recordedStep.servoIndex].servo.write(currentServoPosition);
      serverInput.textAll(servoPins[recordedStep.servoIndex].servoName + "," + currentServoPosition);
      delay(50);
    }
  }
  delay(2000); // Delay before starting the actual steps.
  
  for (int i = 4; i < recordedSteps.size() && playRecordedSteps ; i++)
  {
    RecordedStep &recordedStep = recordedSteps[i];
    delay(recordedStep.delayInStep);
    servoPins[recordedStep.servoIndex].servo.write(recordedStep.value);
    serverInput.textAll(servoPins[recordedStep.servoIndex].servoName + "," + recordedStep.value);
  }
}

void setUpPinModes()
{
  for (int i = 0; i < servoPins.size(); i++)
  {
    servoPins[i].servo.attach(servoPins[i].servoPin);
    servoPins[i].servo.write(servoPins[i].initialPosition);    
  }
}

void setup() {
  // put your setup code here, to run once:
  setUpPinModes();
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("APP IP address: ");
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, handleRoot);
  serverInput.onEvent(onServerInputWebSocketEvent);
  server.addHandler(&serverInput);
  server.begin();
  Serial.println("HTTP server started");

}

void loop() {
  // put your main code here, to run repeatedly:
  serverInput.cleanupClients();
  if (playRecordedSteps)
  { 
    playRecordedRobotArmSteps();
  }

}
