const net = require('net');

const HOST = '127.0.0.1'; // Or the IP address of your server
const PORT = 9192;       // The port your server is listening on

// Create a new TCP client
const client = new net.Socket();
// const msg = 'Hello from the Node.js client!';
const msg = 'JGET START';

// Connect to the server
client.connect(PORT, HOST, () => {
    console.log(`Connected to server: ${HOST}:${PORT}`);
    // Send data to the server
//    client.write('Hello from the Node.js client!');
    client.write(msg);
});

// Handle data received from the server
client.on('data', (data) => {
    console.log(`Received from server: ${data.toString()}`);
    // Close the connection after receiving data (optional)
    if( data.toString() == msg ) {
        console.log('Talking to myself');
    }
//    } else {
//        client.end();
//    }
});

// Handle when the server closes the connection
client.on('end', () => {
    console.log('Disconnected from server');
});

// Handle errors
client.on('error', (err) => {
    console.error(`Client error: ${err.message}`);
});

// Handle connection timeout (optional)
client.on('timeout', () => {
    console.log('Connection timed out');
    client.end();
});

