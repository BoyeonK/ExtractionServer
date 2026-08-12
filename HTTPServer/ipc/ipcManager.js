const net = require('net');
const protobuf = require("protobufjs");
const path = require('path');

const SOCKET_PATH = '/tmp/https.sock';
const protoPath = path.join(__dirname, '../IPC_HTTP.proto');

const PKT_ID_M2H_WELCOME = 0;
const PKT_ID_H2M_WELCOME = 1;
const PKT_ID_H2M_MATCH_MAKE = 2;
const PKT_ID_H2M_MATCH_MAKE_CANCEL = 3;
const H2M2D_BIND_CLIENT_IP_TO_SESSION = 4;

let ipcClient = null;
let rootProto = null;

let receiveBuffer = Buffer.alloc(0);

function initIPC() {
    protobuf.load(protoPath, (err, root) => {
        if (err) {
            console.error("H3 - X : IPC_HTTP.proto 경로 잘못됨.", err);
            return;
        }
        
        rootProto = root;
        ipcClient = net.createConnection(SOCKET_PATH, () => {
            console.log('H3-1 - OK : HTTP -> 메인프로세스 IPC 연결 성공');
            const randomInt = Math.floor(Math.random() * 10000);

            console.log(`H3-2 : ${randomInt}을 테스트 패킷에 전송. C++ 응답 확인 요망`);
            sendHttpWelcome(randomInt);
        });

        ipcClient.on('error', (err) => console.error('H3 - X : IPC 에러:', err.message));
    
        ipcClient.on('data', (data) => {
            receiveBuffer = Buffer.concat([receiveBuffer, data]);

            while (receiveBuffer.length >= 4) {    
                const pktId = receiveBuffer.readUInt16LE(0); 
                const totalPacketSize = receiveBuffer.readUInt16LE(2); 

                if (totalPacketSize < 4) {
                    console.error(`IPC에러, 패킷 크기가 4바이트 미만이라고 '주장'하는 패킷이 들어옴: ${totalPacketSize}`);
                    receiveBuffer = Buffer.alloc(0);
                    break;
                }

                if (receiveBuffer.length < totalPacketSize) {
                    break;
                }

                const payload = receiveBuffer.subarray(4, totalPacketSize);

                handleIncomingPacket(pktId, payload);

                receiveBuffer = receiveBuffer.subarray(totalPacketSize);
            }
        });
    });
}

function handleIncomingPacket(pktId, payload) {
    try {
        if (pktId === PKT_ID_M2H_WELCOME) {
            const MainWelcome = rootProto.lookupType("IPC_Protocol.M2HWelcome");
            const message = MainWelcome.decode(payload);
            console.log(`[Node.js IPC] 수신: MainWelcome (echo: ${message.echo_message})`);
        } else {
            console.warn(`[Node.js IPC] 알 수 없는 패킷 ID 수신: ${pktId}`);
        }
    } catch (err) {
        console.error(`[Node.js IPC] 패킷 디코딩 실패 (ID: ${pktId}):`, err);
    }
}

function sendToCpp(buffer) {
    if (ipcClient && !ipcClient.destroyed) {
        ipcClient.write(buffer);
    } else {
        console.error('IPC 통신 불가: C++ 서버와 연결되어 있지 않습니다.');
    }
}

// Header 4byte + Payload
function makePacket(pktId, payloadBuffer) {
    const header = Buffer.alloc(4);
    const totalSize = 4 + payloadBuffer.length; 

    header.writeUInt16LE(pktId, 0);
    header.writeUInt16LE(totalSize, 2);

    return Buffer.concat([header, payloadBuffer]);
}

function sendHttpWelcome(echoNum) {
    if (!rootProto) return;
    const HttpWelcome = rootProto.lookupType("IPC_Protocol.H2MWelcome");
    const payload = HttpWelcome.encode(HttpWelcome.create({ echoMessage: echoNum })).finish();

    sendToCpp(makePacket(PKT_ID_H2M_WELCOME, payload));
}

function sendHttpMatchMake(ticketId) {
    if (!rootProto) return;

    const HttpMatchMake = rootProto.lookupType("IPC_Protocol.H2MMatchMake");
    const message = HttpMatchMake.create({ ticketRedisKey: ticketId });
    const payload = HttpMatchMake.encode(message).finish();

    sendToCpp(makePacket(PKT_ID_H2M_MATCH_MAKE, payload));

    // TEMP : 이거 빌드할때는 주석처리
    console.log(`매치 테스트 2 - O : ticketId를 IPC를 통해 전송 ticket: ${ticketId}`);
}

function sendHttpMatchMakeCancel(ticketId) {
    if (!rootProto) return;

    const HttpMatchMakeCancel = rootProto.lookupType("IPC_Protocol.H2MMatchMakeCancel");
    const message = HttpMatchMakeCancel.create({ ticketRedisKey: ticketId });
    const payload = HttpMatchMakeCancel.encode(message).finish();

    sendToCpp(makePacket(PKT_ID_H2M_MATCH_MAKE_CANCEL, payload));

    // TEMP : 이거 빌드할때는 주석처리
    console.log(`매치 취소 테스트 2 - 0 : ticketId를 IPC를 통해 전송 ticket: ${ticketId}`);
}

function sendH2M2DBindClientIpToSession(token, ip) {
    if (!rootProto) return;

    const H2M2Dpkt = rootProto.lookupType("IPC_Protocol.H2M2DBindClientIpToSession");
    const message = H2M2Dpkt.create({ token: token, ip: ip});
    const payload = H2M2Dpkt.encode(message).finish();

    sendToCpp(makePacket(H2M2D_BIND_CLIENT_IP_TO_SESSION, payload));

    // TEMP : 이거 빌드할때는 주석처리
    console.log(`매치 테스트 11-1 : [HTTPS 프로세스] token과 ip를 HTTPS프로세스에서 메인프로세스에 전송`);
}

module.exports = {
    initIPC,
    sendHttpMatchMake,
    sendHttpMatchMakeCancel,
    sendH2M2DBindClientIpToSession,
};