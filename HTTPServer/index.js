const path = require('path');
const express = require('express');
require('dotenv').config({ path: path.join(__dirname, '.env') });

const { connectRedis } = require('./config/redisClient');
const { initIPC } = require('./ipc/ipcManager');
const { makeResponse } = require('./utils/response');

const authRoutes = require('./routes/auth');
const itemsRoutes = require('./routes/items');
const matchRoutes = require('./routes/match');

const app = express();
const port = process.env.EXPRESS_PORT || 3000;
app.use(express.json());

connectRedis();
initIPC();

if (process.env.IS_LOCAL_TEST === 'Y') {
    const swaggerUi = require('swagger-ui-express');
    const fs = require('fs');
    const YAML = require('yaml');

    const yamlFile = fs.readFileSync(path.join(__dirname, 'http-api-spec.yaml'), 'utf8');
    const swaggerDocument = YAML.parse(yamlFile);
    app.use('/api-docs', swaggerUi.serve, swaggerUi.setup(swaggerDocument));
}

app.get('/api/version', (req, res) => {
    res.status(200).json(makeResponse(true, 200, { latestVersion: "alphaTest", isMaintenance: false }));
});
app.use('/api', authRoutes);
app.use('/api/items', itemsRoutes);
app.use('/api/game/match', matchRoutes);

// 서버 실행
app.listen(port, '0.0.0.0', () => {
    console.log(`H1 - OK : HTTP서버 클라이언트의 요청 대기중`);
});