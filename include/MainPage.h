#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      alert("Saved value to ESP SPIFFS");
      setTimeout(function(){ document.location.reload(false); }, 500);
    }
  </script></head><body>
  <form action="/get" target="hidden-form">
    inputString (current value %inputString%): <input type="text" name="inputString">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    inputInt (current value %inputInt%): <input type="number " name="inputInt">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    inputFloat (current value %inputFloat%): <input type="number " name="inputFloat">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form>
  <div id="data">0</div>
  <iframe style="display:none" name="hidden-form"></iframe>
  <script>

    setInterval(function() {
      // Call a function repetatively with 2 Second interval
      getData();
    }, 2000); //2000mSeconds update rate // HERE DELAY

    function getData() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("data").innerHTML =
          this.responseText;
        }
      };
      xhttp.open("GET", "readData", true);
      xhttp.send();
    }
  </script>
</body></html>)rawliteral";
const char index_html2[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta http-equiv="X-UA-Compatible" content="IE=edge" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <link
      href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&amp;display=swap"
      rel="stylesheet"
    />
    <link rel="stylesheet" type="text/css" href="/stylesheets/main.css" />
    <title>E-10</title>
  </head>
  
  <style>
    
  </style>
  
  <body>
    <div class="topnav">
      <div class="bar" style="display: none">
         <!-- <a class="setTarget" href="#" onclick="setDiameter();return false;"
          >Target</a
        > -->
        <div class="targetForm">
          <label for="inputDiameter">Diameter:</label>
          <input type="number" id="inputDiameter" placeholder="%inputDiameter%" name="inputDiameter" />
          <a class="setTarget" href="#" onclick="setDiameter();return false;"
            >Submit</a
          >
        </div>
      </div>
      <a class="accordion" href="#" onclick="toggle();return false;">+</a>
    </div>
    <main>
        <div>
            <div class="heading">E-10</div>
            <div class="data">
              <div class="value" id="data">180</div>
              <div class="unit">cm</div>
            </div>
        </div>
    </main>
    <script>
      let page = document.querySelector("body");
      let el = document.querySelector(".bar");
      function toggle() {
        if (el.style.display === "none") {
          el.style.display = "flex";
        } else {
          el.style.display = "none";
        }
      }
      setInterval(function () {
        // Call a function repetatively
        getData();
      }, 600); // update rate // HERE DELAY

      function getData() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.querySelector(".value").innerHTML = this.responseText;
            let number = +this.responseText;
            //if (number < 20) {
            //  window.navigator.vibrate([200, 50, 200, 50, 200, 50, 200, 50, 200]);
            //  console.log('vibrate');
            //}
            //page.style.backgroundColor = number < 20 ? "orangered" : "greenyellow";
          }
        };
        xhttp.open("GET", "readData", true);
        xhttp.send();
      }
      function setDiameter() {
        let inputDiameter = document.querySelector("#inputDiameter");
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            //document.getElementById("data").innerHTML = this.responseText;
            //document.querySelector("body > main > div").remove();
            //document.querySelector('main').innerHTML=this.responseText;
            //let number = this.responseText;
            inputDiameter.value = "";
            inputDiameter.setAttribute('placeholder', this.responseText);
            console.log("target is set");
          }
        };
        
        let url = `setDiameter?inputDiameter=${inputDiameter.value}`;
        //console.log(params);
        xhttp.open("GET", url, true);
        xhttp.send(null);
      }
    </script>
  </body>
</html>

  )rawliteral";