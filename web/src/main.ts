import { createApp } from "vue";
import App from "./App.vue";
import "./style.css";

try {
    const app = createApp(App);
    app.config.errorHandler = (err, instance, info) => {
        document.body.innerHTML = `<div style="color:red;font-size:20px;padding:20px;">Vue Error: ${err}</div>`;
        console.error(err);
    };
    app.mount("#app");
} catch (e) {
    document.body.innerHTML = `<div style="color:red;font-size:20px;padding:20px;">Init Error: ${e}</div>`;
    console.error(e);
}

window.addEventListener('unhandledrejection', function (event) {
    document.body.innerHTML = `<div style="color:red;font-size:20px;padding:20px;">Unhandled Promise Rejection: ${event.reason}</div>`;
});
window.addEventListener('error', function (event) {
    document.body.innerHTML += `<div style="color:red;font-size:20px;padding:20px;">Error: ${event.message}</div>`;
});
