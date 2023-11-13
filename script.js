var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var chart; // Define the chart variable here
var tempData = [];
var moistureData = [];
var waterData = [];

// Create the chart
const labels = Array.from({ length: 10 }, (_, i) => i + 1);

// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();

    var ctx = document.getElementById('weatherChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Temperature (C)',
                    borderColor: 'rgb(186, 231, 225)',
                    data: tempData,
                    yAxisID: 'temperature',  // Associate with the 'temperature' axis
                    fill: false,
                },
                {
                    label: 'Moisture (%)',
                    borderColor: 'rgb(250, 231, 179)',
                    data: moistureData,
                    yAxisID: 'moisture',    // Associate with the 'moisture' axis
                    fill: false,
                },
                {
                    label: 'Water Level (%)',
                    borderColor: 'rgb(234, 185, 180)',
                    data: waterData,
                    yAxisID: 'moisture',    // Associate with the 'moisture' axis
                    fill: false,
                },
            ],
        },
        options: {
            scales: {
                temperature: {
                    type: 'linear',
                    position: 'left',
                    min: 0,
                    max: 30,
                    ticks: {
                        stepSize: 5,
                    },
                },
                moisture: {
                    type: 'linear',
                    position: 'right',
                    min: 0,
                    max: 100,
                    ticks: {
                        stepSize: 20,
                    },
                },
            },
        },
    });
}

function getReadings() {
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);

    if (myObj.type == "cardData") {

        document.getElementById("moistureLevel").innerHTML = myObj.moistureLevel.toFixed(2);
        document.getElementById("waterLevel").innerHTML = myObj.waterLevel.toFixed(2);
        document.getElementById("waterDate").innerHTML = myObj.waterDate;
        document.getElementById("temperature").innerHTML = myObj.temperature;
    } 
    
    else if (myObj.type == "chartData") {
        var tempData = myObj.temperature;
        var moistureData = myObj.moisture;
        var waterData = myObj.waterLevel;

        // Clear existing data
        chart.data.labels = Array.from({ length: tempData.length }, (_, i) => i + 1);
        chart.data.datasets[0].data = tempData;
        chart.data.datasets[1].data = moistureData;
        chart.data.datasets[2].data = waterData;

        chart.update();
    }
}



