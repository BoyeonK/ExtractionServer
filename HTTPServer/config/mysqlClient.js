const mysql = require('mysql2/promise');

const pool = mysql.createPool({
    host: process.env.MYSQL_HOST,
    port: process.env.MYSQL_PORT,
    user: process.env.MYSQL_USER,
    password: process.env.MYSQL_PASSWORD,
    database: process.env.MYSQL_DATABASE,
    waitForConnections: true,
    connectionLimit: 5,
    queueLimit: 0
});

async function DbConnection() {
    try {
        const connection = await pool.getConnection();
        console.log('H4 - OK : HTTP서버에서 MySQL에 연결 완료');
        connection.release();
    } catch (error) {
        console.error('H4 - X : HTTP서버에서 MySQL에 연결 실패:', error.message);
    }
}

DbConnection();

module.exports = { pool };