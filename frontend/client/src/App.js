import React from "react";
import './App.css';
import MapImage from "./resources/green-map.png";

let latitude = null;
let longitude = null;

function showPosition(position) {
  latitude = position.coords.latitude;
  longitude = position.coords.longitude;
}



function App() {
  const [mode, setMode] = React.useState(0);
  const [locX, setLocX] = React.useState(0);
  const [locY, setLocY] = React.useState(0);
  const [isTracked, setIsTracked] = React.useState(false);
  const [pwrIndex, setPwrIndex] = React.useState(-1);

	/*
  React.useEffect(() => {
    fetch("https://api.weather.gov/alerts/active/area/FL", {
      method: "GET",
      headers: new Headers({
        "User-Agent": "(advd, matthewalantaylor2@gmail.com)"
      })
    })
      .then((res) => res.json())
      .then((data) => console.log(data.type))
      .catch((error) => console.log(error));
  }, []);
*/


  if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition(showPosition);
  }

  function setLocation(e) {
    if (navigator.geolocation) {
      navigator.geolocation.getCurrentPosition(showPosition);
    }

    if (latitude !== null && longitude !== null) {
      let xPos = -1 * (-longitude - 130) / 65.0;
      setLocX(xPos * 100);

      let yPos = (latitude - 25) / 25.0;
      setLocY(yPos * 100);

      setIsTracked(true);
	  
	let url = `https://api.weather.com/v2/indices/powerDisruption/daypart/15day?geocode=${latitude},${longitude}&language=en-US&format=json&apiKey=41b54a6111754445b54a611175b44574`;
	fetch(url)
	    .then((res) => res.json())
	    .then((data) => setPwrIndex(data.powerDisruptionIndex12hour.powerDisruptionIndex[0]))
	    .catch((error) => console.log(error));
    }
  }

  function switchMode(e) {
    if (mode === 0) {
      setMode(1);
	if (pwrIndex > 3) {
		 fetch("/api?command=on")
        	.then((res) => res.json())
        	.then((data) => console.log(data))
        	.catch((error) => console.log(error));
	}
	else {
 		fetch("/api?command=off")
        	.then((res) => res.json())
        	.then((data) => console.log(data))
        	.catch((error) => console.log(error));
	}
    }
    else if (mode === 1) {
      setMode(2);
      fetch("/api?command=on")
        .then((res) => res.json())
        .then((data) => console.log(data))
        .catch((error) => console.log(error));
    }
    else if (mode === 2) {
      setMode(0);
      fetch("/api?command=off")
        .then((res) => res.json())
        .then((data) => console.log(data))
        .catch((error) => console.log(error));
    }
  }

  return (
    <React.Fragment>
      <div className="button" id="mode-button" onClick={switchMode}><p>Mode</p></div>
      <table id="indicator-table">
        <tbody>
          <tr>
            <td><div className={"indicator-light" + (mode === 0 ? " on" : "")}></div></td>
            <td><div className={"indicator-light" + (mode === 1 ? " on" : "")}></div></td>
            <td><div className={"indicator-light" + (mode === 2 ? " on" : "")}></div></td>
          </tr>
          <tr>
            <td><p>Off</p></td>
            <td><p>Auto</p></td>
            <td><p>On</p></td>
          </tr>
        </tbody>
      </table>
      <div id="bg-window">
        <h1 id="title">&lt;advantaged/&gt; {pwrIndex === -1 ? "" : "    Power disruption: " + pwrIndex.toString()}</h1>
      </div>
      <div className="centered">
        <div id="img-wrapper">
          <div id="loc-point" className={isTracked ? "" : "hidden"} style={{left: locX.toString() + "%", bottom: locY.toString() + "%"}}></div>
          <img id="map-img" src={MapImage} alt="map" />
        </div>
      </div>
      <div className="button" id="loc-button" onClick={setLocation}><p>Set Location</p></div>
    </React.Fragment>
  );
}

export default App;
