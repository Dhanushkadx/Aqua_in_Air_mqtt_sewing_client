// 
// 
// 

#include "web_socket.h"


// ----------------------------------------------------------------------------
// WebSocket initialization
// ----------------------------------------------------------------------------
#include "typex.h"

AsyncWebSocket ws("/ws");

void notifyClients_pageIndex() {
	File fileToReadx = SPIFFS.open("/system_config.json");
	if (!fileToReadx)
	{
		Serial.println(F("couldn't open file"));
		return;
	}
	
	DynamicJsonDocument docx(13000);
	DeserializationError err = deserializeJson(docx,  fileToReadx);
	fileToReadx.close();
	if (err) {
		Serial.print(F("deserializeJson() failed with code "));
		Serial.println(err.f_str());
		return;
	}

	String data;
	//data.reserve(12288);
	docx["respHeader"]= "pint";
	size_t len = serializeJson(docx, data);
	//ws.textAll(data, len);
	
	/*char buffer[100];
	sprintf(buffer, "{\"zone01\":{\"name\":\"NILUS ROOM\"}}");*/
	Serial.print(F("send contact json and free heap>"));
	Serial.println(ESP.getFreeHeap());
	ws.textAll(data.c_str(),len);
	docx.garbageCollect();
	//Serial.println(data.c_str());
	
}




void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
	AwsFrameInfo *info = (AwsFrameInfo*)arg;
	if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
		
		DynamicJsonDocument json_req(13000);
		DeserializationError err = deserializeJson(json_req, data);
		if (err) {
			Serial.print(F("deserializeJson() failed with code "));
			Serial.println(err.c_str());
			return;
		}
		
		const char *cmd = json_req["command"];
		Serial.println(F("action is command"));
		DynamicJsonDocument json_resp(150);
		

		const char *action = json_req["action"];
		if (strcmp(action, "sys") == 0) {
			Serial.println(F("action is toggle"));
			notifyClients_pageIndex();
		}
		
		String data;
		size_t len = serializeJson(json_resp, data);
		ws.textAll(data.c_str(), len);
		

	}
}

void onEvent(AsyncWebSocket       *server,
AsyncWebSocketClient *client,
AwsEventType          type,
void                 *arg,
uint8_t              *data,
size_t                len) {

	switch (type) {
		case WS_EVT_CONNECT:
		Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
		
		break;
		case WS_EVT_DISCONNECT:
		Serial.printf("WebSocket client #%u disconnected\n", client->id());
		break;
		case WS_EVT_DATA:
		handleWebSocketMessage(arg, data, len);
		break;
		case WS_EVT_PONG:
		case WS_EVT_ERROR:
		break;
	}
}


