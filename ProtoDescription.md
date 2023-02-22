
# Callbot Master Server
Callbot master server

## gRPC Service
```
service SmartIVRPhonegateway
{
    rpc CallToBot (stream SmartIVRRequest) returns (stream SmartIVRResponse);
}
```
#### CallToBot
```
rpc CallToBot (stream SmartIVRRequest) returns (stream SmartIVRResponse);
```
A bidirectional streaming function between Phone Gateway Client and Callbot Master Server
## Protobuf

#### SmartIVRRequest
The top-level message sent by Phone Gateway Client for the `CallToBot` method.

| Fields | Type | Description |
| ------ | ------ | ------- |
| config | `Config`| Provides config information for the request. <br> The first `SmartIVRRequest` message must contain only a `config` message.
| audio_content |`bytes`| The audio data to be recognized. Sequential chunks of audio data are sent in sequential `SmartIVRRequest` messages. <br>The first `SmartIVRRequest` message must not contain `audio_content` data and all subsequent `VoiceBotRequest`messages must contain `audio_content` data.
| key_press |`string`| The pressed key by customer. This is an optional field, only contain value if customer press key, null otherwise. Sequential chunks of audio data are sent in sequential `SmartIVRRequest` messages. <br>The first `SmartIVRRequest` message must not contain `key_press` data.
| is_playing |`bool`| When Phone Gateway is playing audio for customer, the flag is `True` in sequential `SmartIVRRequest` messages. Other wise, the flag is `False`

#### SmartIVRResponse
The message returned to Phone Gateway Client by the `CallToBot` method. A series of zero or more `SmartIVRResponse` messages are streamed back to the client.

| Fields | Type | Description |
| ------ | ------ | ------- |
| status | `Status`| Status of response message
| type | `SmartIVRResponseType`| Type of response message
| text_asr | `string`| The recognized text from customer's audio<br>Only contain value if `type` is `RECOGNIZE` or `RESULT_ASR`
| text_bot | `string`| The answer text from Callbot to customer
| audio_content | `bytes`| The binary of audio data that Callbot speak the answer text <br> Audio is encoded with WAV format
| forward_sip_json | `string`| The json string contain the SIP info to forward call.<br>Only contain value if `type` is `CALL_FORWARD`

#### Config
The configuration of the call center.

| Fields | Type | Description |
| ------ | ------ | ------- |
| conversation_id | `string`| ID of conversation

#### SmartIVRResponseType
The enum definition for response type from Callbot Master Server

| Enum | Value | Description |
| ------ | ------ | ------- |
| RECOGNIZE | 0 | Is set when Callbot is listening to customer audio, for recognizing
| RESULT_ASR | 1 | Is set when Callbot has finished recognizing, and return recognized text
| RESULT_TTS | 2 | Is set when Callbot send text & audio resposne from Callbot to customer
| CALL_WAIT | 3 | Is set when Callbot ask Phone Gateway to hold call with music, for querying API or some long task
| CALL_FORWARD | 4 | Is set when Callbot ask Phone Gateway to transfer call
| CALL_END | 5 | Is set when Callbot ask Phone Gateway to end call

#### Status
The message with status code and the notify message. It can be either success or error.

| Fields | Type | Description |
| ------ | ------ | ------- |
| code | `int32`| Status code <br> If **SUCCESS**, then code = 200 <br> If **ERROR**, then code = -1
| message | `string`| The notify message
