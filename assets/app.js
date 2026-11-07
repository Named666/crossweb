(function () {
  var status = document.getElementById("status");
  var pending = {};

  function setStatus(text) {
    status.textContent = text;
  }

  window.external = window.external || {};
  window.external.onMessage = function (id, result) {
    var done = pending[id];
    if (done) {
      delete pending[id];
      done(result);
    }
  };

  function invoke(cmd, payload) {
    return new Promise(function (resolve, reject) {
      if (!window.external || typeof window.external.invoke !== "function") {
        reject(new Error("bridge missing"));
        return;
      }
      var id = window.external.invoke(cmd, payload);
      if (!id) {
        reject(new Error("bridge rejected"));
        return;
      }
      pending[id] = resolve;
      setTimeout(function () {
        if (pending[id]) {
          delete pending[id];
          reject(new Error("timeout"));
        }
      }, 5000);
    });
  }

  function finish(text, ok) {
    setStatus(text);
    document.title = ok ? "crossweb-smoke-ok" : "crossweb-smoke-fail";
  }

  function selfCheck() {
    invoke("app.ping", { nonce: "ready1" }).then(function (ping) {
      return invoke("fs.read", { path: "/etc/passwd" }).then(function (denied) {
        var pingOk = ping && ping.ok === true && ping.pong === "ready1";
        var fsDenied = denied && denied.ok === false && denied.error === "permission denied";
        if (pingOk && fsDenied) {
          finish("Native ping returned ready1. fs.read was denied.", true);
        } else {
          finish("Unexpected native result.", false);
        }
      });
    }).catch(function () {
      finish("Native call failed.", false);
    });
  }

  document.getElementById("ping").addEventListener("click", selfCheck);
  selfCheck();
})();
