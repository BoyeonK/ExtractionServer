const redis = require('redis');

const redisClient = redis.createClient({
    url: `redis://${process.env.REDIS_HOST}:${process.env.REDIS_PORT}`
});

// sess:<UUID> 와 user_sess:<ID> 는 항상 같은 값을 써야 한다 (redis_keys.md 3번)
const SESSION_TTL_SEC = 900;

redisClient.on('error', (err) => console.error('H2 - X HTTP서버에서 Redis에러 감지', err));

async function connectRedis() {
    try {
        await redisClient.connect();
        console.log('H2 - OK : HTTP서버 Redis 연결 성공');
    } catch (err) {
        console.error('H2 - X : HTTP서버 Redis 연결 실패:', err);
    }
}

// sess:<UUID>와 user_sess:{userId}의 TTL을 원자적으로 갱신
const refreshSessionScript = `
    local ttl = tonumber(ARGV[1])
    local userId = redis.call('HGET', KEYS[1], 'user_id')
    if not userId then return 0 end
    redis.call('EXPIRE', KEYS[1], ttl)
    redis.call('EXPIRE', 'user_sess:' .. userId, ttl)
    return 1
`;

async function refreshSession(sessionId, ttl = SESSION_TTL_SEC) {
    return redisClient.eval(refreshSessionScript, {
        keys: [sessionId],
        arguments: [ttl.toString()]
    });
}

module.exports = {
    redisClient,
    connectRedis,
    refreshSession,
    SESSION_TTL_SEC,
};