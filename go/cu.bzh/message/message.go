package message

import (
	"bytes"
	"compress/zlib"
	"encoding/binary"
	"errors"
	"io"
	"io/ioutil"
)

//
// BinaryMessage is simply a binary blog.
// When sent over the wire, it is prefixed by a header having the following structure:
// -----
// uint16: compressed size of message binary blob
// uint16: uncompressed size of message binary blob
// -----
//
// The binary blob itself has the following structure
// ---
// uint8   : action byte
// uint8   : sender ID
// uint8   : Recepients IDs count
// []uint8 : Recepients IDs
//
//
//
// ---

// WireMessage ...
type WireMessage struct {
	CompressedSize   uint16 // this field can probably be removed
	UncompressedSize uint16
	CompressedBytes  []byte
}

// Message ...
type Message struct {
	// this []byte contains the ActionByte, the SenderID, the Receivers, and additional message-specific bytes
	UncompressedBytes []byte
}

// ActionByte ...
func (m *Message) ActionByte() (ActionByte, error) {
	b, err := bytes.NewReader(m.UncompressedBytes).ReadByte()
	return (ActionByte)(b), err
}

// SenderID ...
func (m *Message) SenderID() (uint8, error) {
	r := bytes.NewReader(m.UncompressedBytes)
	offset, err := r.Seek(1, io.SeekStart)
	if err != nil {
		return 0, err
	}
	if offset != 1 {
		return 0, err
	}
	return r.ReadByte()
}

// RecipientIds ...
func (m *Message) RecipientIds() ([]uint8, error) {
	// fmt.Println(len(m.UncompressedBytes), m.UncompressedBytes)
	r := bytes.NewReader(m.UncompressedBytes)
	// read recipient count
	offset, err := r.Seek(2, io.SeekStart)
	if err != nil {
		return nil, err
	}
	if offset != 2 {
		return nil, err
	}
	var count uint8
	count, err = r.ReadByte()
	if err != nil {
		return nil, err
	}
	var recipients = make([]uint8, count)
	for i := uint8(0); i < count; i++ {
		id, err := r.ReadByte()
		if err != nil {
			return nil, err
		}
		recipients[i] = id
	}
	return recipients, nil
}

// Script extracts and return the lua script bytes from a "ActionScript" message
func (m *Message) Script() ([]byte, error) {
	actionByte, err := m.ActionByte()
	if err != nil {
		return nil, err
	}
	if actionByte != ActionScript {
		return nil, errors.New("Script() can only be called on an ActionScript message")
	}
	if len(m.UncompressedBytes) < 6 {
		return nil, errors.New("ActionScript message cannot be shorter than 6 bytes")
	}

	// read script buffer size
	var size = binary.LittleEndian.Uint32(m.UncompressedBytes[2:])
	// check that the buffer is large enough to hold all the expected data
	if uint32(len(m.UncompressedBytes)) < (6 + size) {
		return nil, errors.New("ActionScript message size problem, it might be corrupted")
	}

	return m.UncompressedBytes[6:], nil
}

// Clock extracts and returns the clock value from an ActionClock message
func (m *Message) Clock() (uint32, error) {
	actionByte, err := m.ActionByte()
	if err != nil {
		return 0, err
	}
	if actionByte != ActionClock {
		return 0, errors.New("Script() can only be called on an ActionScript message")
	}
	if len(m.UncompressedBytes) != 6 {
		return 0, errors.New("ActionClock message should be 6 bytes")
	}

	// read clock time (uint32 milliseconds)
	var clock = binary.LittleEndian.Uint32(m.UncompressedBytes[2:])

	return clock, nil
}

// PlayerHasArrivedUsernameAndUserID extracts and returns the player's username and userID
// contained in a ActionPlayerHasArrived message
func (m *Message) PlayerHasArrivedUsernameAndUserID() ([]byte, []byte, error) {
	const messageMinimumLength int = 4

	actionByte, err := m.ActionByte()
	if err != nil {
		return nil, nil, err
	}
	if actionByte != ActionPlayerHasArrived {
		return nil, nil, errors.New("PlayerHasArrivedUsername() can only be called on an ActionPlayerHasArrived message")
	}
	if len(m.UncompressedBytes) < messageMinimumLength {
		return nil, nil, errors.New("ActionPlayerHasArrived message cannot be shorter than 4 bytes")
	}

	// read username length
	var usernameLen uint8 = []uint8(m.UncompressedBytes)[2]
	// read userID length
	var userIDLen uint8 = []uint8(m.UncompressedBytes)[3+usernameLen]

	// check that the buffer is large enough to hold all the expected data
	if len(m.UncompressedBytes) < (messageMinimumLength + int(usernameLen) + int(userIDLen)) {
		return nil, nil, errors.New("ActionPlayerHasArrived message size problem, it might be corrupted")
	}

	return m.UncompressedBytes[3 : 3+usernameLen], m.UncompressedBytes[4+usernameLen : 4+usernameLen+userIDLen], nil
}

// Decoder ...
type Decoder struct {
	r          io.Reader
	twoByteBuf []byte
}

// NewDecoder ...
func NewDecoder(r io.Reader) *Decoder {
	return &Decoder{
		r:          r,
		twoByteBuf: make([]byte, 2),
	}
}

// DecodeWireMessage reads the next WireMessage
func (md *Decoder) DecodeWireMessage() (*WireMessage, error) {

	var wireMessage = WireMessage{
		CompressedSize:   0,
		UncompressedSize: 0,
		CompressedBytes:  nil,
	}

	var n int
	var err error

	// read compressed size value (uint16)
	n, err = io.ReadFull(md.r, md.twoByteBuf)
	if err != nil {
		return nil, err
	}
	if n != len(md.twoByteBuf) {
		return nil, errors.New("failed to read compressed size value")
	}
	wireMessage.CompressedSize = binary.LittleEndian.Uint16(md.twoByteBuf)

	// read uncompressed size value (uint16)
	n, err = io.ReadFull(md.r, md.twoByteBuf)
	if err != nil {
		return nil, err
	}
	if n != len(md.twoByteBuf) {
		return nil, errors.New("failed to read uncompressed size value")
	}
	wireMessage.UncompressedSize = binary.LittleEndian.Uint16(md.twoByteBuf)

	// read compressed bytes
	wireMessage.CompressedBytes = make([]byte, wireMessage.CompressedSize) // destination buffer for compressed bytes
	n, err = io.ReadFull(md.r, wireMessage.CompressedBytes)
	if err != nil {
		return nil, err
	}
	if n != len(wireMessage.CompressedBytes) {
		return nil, errors.New("failed to read compressed bytes")
	}

	return &wireMessage, nil
}

// DecodeMessage reads the next WireMessage from the Reader and convert it into Message
func (md *Decoder) DecodeMessage() (*Message, error) {

	var wireMessage, err = md.DecodeWireMessage()
	if err != nil {
		return nil, err
	}

	// uncompress bytes and construct result Message struct
	var bytesReader = bytes.NewReader(wireMessage.CompressedBytes)
	var zlibReadCloser io.ReadCloser
	zlibReadCloser, err = zlib.NewReader(bytesReader)
	if err != nil {
		return nil, err
	}
	defer zlibReadCloser.Close()

	var message = &Message{
		UncompressedBytes: nil,
	}
	message.UncompressedBytes, err = ioutil.ReadAll(zlibReadCloser)
	if err != nil {
		return nil, err
	}
	if int(wireMessage.UncompressedSize) != len(message.UncompressedBytes) {
		return nil, errors.New("uncompressed size mismatch")
	}

	return message, nil
}

// Encoder ...
type Encoder struct {
	w io.Writer
}

// NewEncoder ...
func NewEncoder(w io.Writer) *Encoder {
	return &Encoder{
		w: w,
	}
}

// EncodeWireMessage ...
func (me *Encoder) EncodeWireMessage(wm *WireMessage) error {

	var n int
	var err error
	var twoByteBuf = make([]byte, 2)

	// write compressed size
	binary.LittleEndian.PutUint16(twoByteBuf, wm.CompressedSize)
	n, err = me.w.Write(twoByteBuf)
	if err != nil {
		return err
	}
	if n != len(twoByteBuf) {
		return errors.New("failed to write compressed size")
	}

	// write uncompressed size
	binary.LittleEndian.PutUint16(twoByteBuf, wm.UncompressedSize)
	n, err = me.w.Write(twoByteBuf)
	if err != nil {
		return err
	}
	if n != len(twoByteBuf) {
		return errors.New("failed to write uncompressed size")
	}

	// write bytes
	r := bytes.NewReader(wm.CompressedBytes)
	var copiedBytesCount int64
	copiedBytesCount, err = io.Copy(me.w, r)
	if err != nil {
		return err
	}
	if copiedBytesCount != int64(wm.CompressedSize) {
		return errors.New("failed to write compressed bytes")
	}

	return err
}

// EncodeMessage ...
func (me *Encoder) EncodeMessage(m *Message) error {

	// convert Message into WireMessage
	var compressedByteBuffer bytes.Buffer
	w := zlib.NewWriter(&compressedByteBuffer)
	w.Write(m.UncompressedBytes)
	w.Close()

	var wireMessage = &WireMessage{
		CompressedSize:   uint16(compressedByteBuffer.Len()),
		UncompressedSize: uint16(len(m.UncompressedBytes)),
		CompressedBytes:  compressedByteBuffer.Bytes(),
	}

	// TODO: stream zip
	return me.EncodeWireMessage(wireMessage)
}

// ActionByte ...
type ActionByte byte

const (
	// ActionPing is used to ping the server.
	ActionPing ActionByte = 0

	// ActionTransmitClientID is used to attribute
	// an ID to a client.
	ActionTransmitClientID ActionByte = 1

	// ActionAckClientID ...
	ActionAckClientID ActionByte = 2

	// ActionPlayerInfo carries player information
	// (position, speed, etc.)
	ActionPlayerInfo ActionByte = 3

	// ActionPlayerHasArrived notifies of a player arriving on the server
	ActionPlayerHasArrived ActionByte = 4

	// ActionPlayerHasLeft notifies of a player leaving the server
	ActionPlayerHasLeft ActionByte = 5

	// ActionPlayerSayGlobal is a player chat message
	ActionPlayerSayGlobal ActionByte = 6

	// ActionScript is used by a client to submit a new script,
	// and by the server to send the script to all clients.
	ActionScript ActionByte = 7

	// ActionReadyToRunScript is sent by a client when script
	// and all dependencies have been loaded
	ActionReadyToRunScript ActionByte = 8

	// ActionStartScript is sent by server to client to start the script
	ActionStartScript ActionByte = 9

	// ActionGameEvent ...
	ActionGameEvent ActionByte = 10

	// ActionClockRequest ...
	ActionClockRequest ActionByte = 11

	// ActionClock ...
	ActionClock ActionByte = 12

	// ActionClockFailed ...
	ActionClockFailed = 13

	// ActionAckClockSync ...
	ActionAckClockSync = 14

	// ActionLog ...
	ActionLog = 15

	// ActionPlayerState ...
	ActionPlayerState = 16

	// ActionCubeReplace ...
	ActionCubeReplace = 17

	// ActionCubeAdd ...
	ActionCubeAdd = 18

	// ActionCubeRemove ...
	ActionCubeRemove = 19

	// ActionDisconnected is emitted locally when disconnected
	ActionDisconnected = 20
)

// ActionByteMap ...
var ActionByteMap = map[byte]ActionByte{
	0:  ActionPing,
	1:  ActionTransmitClientID,
	2:  ActionAckClientID,
	3:  ActionPlayerInfo,
	4:  ActionPlayerHasArrived,
	5:  ActionPlayerHasLeft,
	6:  ActionPlayerSayGlobal,
	7:  ActionScript,
	8:  ActionReadyToRunScript,
	9:  ActionStartScript,
	10: ActionGameEvent,
	11: ActionClockRequest,
	12: ActionClock,
	13: ActionClockFailed,
	14: ActionAckClockSync,
	15: ActionLog,
	16: ActionPlayerState,
	17: ActionCubeReplace,
	18: ActionCubeAdd,
	19: ActionCubeRemove,
	20: ActionDisconnected,
}

// ActionHasSender ...
var ActionHasSender = map[ActionByte]bool{
	ActionPing:             true,
	ActionTransmitClientID: true,
	ActionAckClientID:      true,
	ActionPlayerInfo:       true,
	ActionPlayerHasArrived: true,
	ActionPlayerHasLeft:    true,
	ActionPlayerSayGlobal:  true,
	ActionScript:           true,
	ActionReadyToRunScript: true,
	ActionStartScript:      false,
	ActionGameEvent:        true,
	ActionClockRequest:     true,
	ActionClock:            true,
	ActionClockFailed:      true,
	ActionAckClockSync:     true,
	ActionLog:              true,
	ActionPlayerState:      true,
	ActionCubeReplace:      true,
	ActionCubeAdd:          true,
	ActionCubeRemove:       true,
	ActionDisconnected:     false,
}

// // This could be used for debugging
// // ActionByteName name string ...
// var ActionByteName = map[ActionByte]string{
// 	ActionPing:             "ActionPing",
// 	ActionTransmitClientID: "ActionTransmitClientID",
// 	ActionAckClientID:      "ActionAckClientID",
// 	ActionPlayerInfo:       "ActionPlayerInfo",
// 	ActionPlayerHasArrived: "ActionPlayerHasArrived",
// 	ActionPlayerHasLeft:    "ActionPlayerHasLeft",
// 	ActionPlayerSayGlobal:  "ActionPlayerSayGlobal",
// 	ActionScript:           "ActionScript",
// 	ActionReadyToRunScript: "ActionReadyToRunScript",
// 	ActionStartScript:      "ActionStartScript",
//  ActionGameEvent:        "ActionGameEvent",
// }

//------------------------------
// Messages built by the server
//------------------------------

// NewMessageTransmitClientID ...
func NewMessageTransmitClientID(clientID uint8) (*Message, error) {

	var buf bytes.Buffer
	// buf size = action (1) + sender (1) + clientID (1)

	// action
	err := buf.WriteByte((byte)(ActionTransmitClientID))
	if err != nil {
		return nil, err
	}

	// sender
	err = buf.WriteByte(0)
	if err != nil {
		return nil, err
	}

	// client id
	err = buf.WriteByte(clientID)
	if err != nil {
		return nil, err
	}

	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}

	return message, nil
}

// NewMessagePlayerHasArrived allocates a Message for notifying that a player arrived
// on the server. The senderID is the ID of the player who arrived.
func NewMessagePlayerHasArrived(arrivingPlayerID uint8) (*Message, error) {
	// buf size = 2 // action (1) + sender(arrivingPlayerID) (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionPlayerHasArrived))
	if err != nil {
		return nil, err
	}
	// sender ID
	err = buf.WriteByte(arrivingPlayerID)
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessagePlayerHasLeft allocates a Message for notifying that a player left the
// server. The senderID is the ID of the player who left.
func NewMessagePlayerHasLeft(leavingPlayerID uint8) (*Message, error) {
	// buf size = 2 // action (1) + sender(leavingPlayerID) (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionPlayerHasLeft))
	if err != nil {
		return nil, err
	}
	// sender ID
	err = buf.WriteByte(leavingPlayerID)
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessageStartScript is sent by the server to clients to tell them to start the Lua script on their end.
func NewMessageStartScript() (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionStartScript))
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessageScript is used to send a Lua script
func NewMessageScript(script string) (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionScript))
	if err != nil {
		return nil, err
	}
	// sender
	err = buf.WriteByte(0)
	if err != nil {
		return nil, err
	}

	l := uint32(len(script))

	err = binary.Write(&buf, binary.LittleEndian, l)
	if err != nil {
		return nil, err
	}

	n, err := buf.WriteString(script)
	if err != nil {
		return nil, err
	}

	if n != int(l) {
		return nil, errors.New("wrong string size written")
	}

	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessageClockRequest is sent by the client to request server clock
func NewMessageClockRequest(clientID uint8) (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionClockRequest))
	if err != nil {
		return nil, err
	}
	// sender
	err = buf.WriteByte(clientID)
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessageClock is sent by the server, answering ClockRequest.
// It gives the local time on the server.
func NewMessageClock(t uint32) (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionClock))
	if err != nil {
		return nil, err
	}
	// sender
	err = buf.WriteByte(0)
	if err != nil {
		return nil, err
	}
	// write time
	err = binary.Write(&buf, binary.LittleEndian, t)
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

// NewMessageClockFailed, generated locally to replace MessageClock
// When the latency is too important.
func NewMessageClockFailed() (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionClockFailed))
	if err != nil {
		return nil, err
	}
	// sender
	err = buf.WriteByte(0)
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}

func NewMessageDisconnected() (*Message, error) {
	// buf size = 1 // action (1)
	var buf bytes.Buffer
	// action
	err := buf.WriteByte((byte)(ActionDisconnected))
	if err != nil {
		return nil, err
	}
	// allocate Message
	var message = &Message{
		UncompressedBytes: buf.Bytes(),
	}
	return message, nil
}
