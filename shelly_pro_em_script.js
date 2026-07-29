// Shelly Pro EM 50A - High-Frequency MQTT Bridge
// Fast 500ms updates to prevent cosmetic mismatches on Hoymiles S-Miles app

let CONFIG = {
  update_interval_ms: 500, // Publish every 500ms
  mqtt_topic: "shelly-em/realtime"
};

// Check if MQTT is configured and connected
if (!MQTT.isConnected()) {
  print("MQTT is not connected! Please configure MQTT in Shelly settings.");
} else {
  print("Starting High-Frequency Telemetry Bridge...");
  
  Timer.set(CONFIG.update_interval_ms, true, function() {
    // Get status of the first phase (em1:0)
    let em_status = Shelly.getComponentStatus("em1", 0);
    
    if (em_status === null) {
      return;
    }

    // Minified JSON payload to reduce network latency
    let payload = JSON.stringify({
      "v": em_status.voltage,
      "c": em_status.current,
      "p": em_status.act_power,
      "s": em_status.aprt_power,
      "pf": em_status.pf,
      "f": em_status.freq
    });

    // Publish to MQTT broker (QoS 0, Retain false for max speed)
    MQTT.publish(CONFIG.mqtt_topic, payload, 0, false);
  });
}
