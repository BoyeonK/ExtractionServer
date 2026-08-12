#include "IoUringWrapper.h"
#include "IOTask.h"
#include "ObjectPool.h"

IoUringWrapper::IoUringWrapper() {
    int ret = io_uring_queue_init(4096, &_ring, 0);
    if (ret < 0) {
        std::cerr << "링을 만들수가 없엉 : " << ret << std::endl;
        exit(1);
    }
}

IoUringWrapper::~IoUringWrapper() {
    io_uring_queue_exit(&_ring);
}

bool IoUringWrapper::ExecuteCQTask() {
    bool ret = false;
    struct io_uring_cqe* cqe = nullptr;

    while (io_uring_peek_cqe(&_ring, &cqe) == 0) {
        if (!cqe) break;

        IOTask* task = reinterpret_cast<IOTask*>(io_uring_cqe_get_data(cqe));
        if (task) task->callback(cqe->res);

        io_uring_cqe_seen(&_ring, cqe);
        ret = true;
    }
    return ret;
}

void IoUringWrapper::RegisterRecv(int fd, void* buf, int32_t len, IOTask* task) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&_ring);
    if (!sqe) {
        return;
    }

    io_uring_prep_read(sqe, fd, buf, len, 0);
    io_uring_sqe_set_data(sqe, task);

    io_uring_submit(&_ring);
}

void IoUringWrapper::RegisterIPCSendTask(IPCSendTask* pIPCSendTask) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&_ring);
    if (!sqe) {
        ObjectPool<SendBuffer>::Release(pIPCSendTask->pBuffer);
        ObjectPool<IPCSendTask>::Release(pIPCSendTask);
        return;
    }

    io_uring_prep_send(sqe, pIPCSendTask->fd, pIPCSendTask->pBuffer->Buffer(), pIPCSendTask->pBuffer->WriteSize(), 0);
    io_uring_sqe_set_data(sqe, pIPCSendTask);

    io_uring_submit(&_ring);
}

void IoUringWrapper::RegisterRecvMsg(int fd, struct msghdr* msg, IOTask* task) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&_ring);
    if (!sqe) {
        return;
    }

    io_uring_prep_recvmsg(sqe, fd, msg, 0);
    io_uring_sqe_set_data(sqe, task);
    
    io_uring_submit(&_ring);
}

void IoUringWrapper::RegisterSendMsg(int fd, struct msghdr* msg, IOTask* task) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&_ring);
    if (!sqe) {
        return;
    }

    io_uring_prep_sendmsg(sqe, fd, msg, 0);
    io_uring_sqe_set_data(sqe, task);

    io_uring_submit(&_ring);
}

void IoUringWrapper::RegisterAcceptTask(int listenFd, IOTask* task) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&_ring);
    if (!sqe) return;

    io_uring_prep_accept(sqe, listenFd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, task);
    
    io_uring_submit(&_ring);
}

SendBuffer* IoUringWrapper::OpenSendBuffer(uint32_t allocSize) {
    if (allocSize > SendBufferChunk::SEND_BUFFER_CHUNK_SIZE) {
        throw std::runtime_error("writeSize > _allocSize");
    }

	if (_sendBufferChunkRef == nullptr or _sendBufferChunkRef->FreeSize() < allocSize) {
		std::shared_ptr<SendBufferChunk> newChunk = std::make_shared<SendBufferChunk>();
		_sendBufferChunkRef = newChunk;
	}

    if (_sendBufferChunkRef->IsOpen() == true) {
        throw std::runtime_error("sendBuffer Error");
    }

	return _sendBufferChunkRef->Open(allocSize);
}