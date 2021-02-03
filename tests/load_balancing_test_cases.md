# Setting up load balancing

1. Power on both devices and open the web gui
2. Enable MQTT and give each device a unique Base-topic
3. Enable load balancing in the 'Setup' box under the 'OpenEVSE' tab
4. Set 'Max Current total' to the amount of current available for booth devices to share
5. Set 'Max safe Current' to the amount of current available for each device when both are awake
6. Set 'Balance with' on each device to the MQTT Base-topic of the other device

# Testing load balancing

| Test Case ID | Test Scenario | Test Steps | Expected Result |
| -----------: |---------------| -----------| ----------------|
| 1 | Both devices are sleeping | 1. Power on both devices <br/>2. Make sure both devices are sleeping (if not, click the 'pause' button in the 'Charge Options' box under the 'OpenEVSE' tab in the web gui). | Both devices should have their max current set to the 'Max Current total' value.
| 2 | Only one device is awake | 1. Power on both devices <br/>2. Activate one of the devices (either scan a registered RFID tag or click the 'Start' button in the 'Charge Options' box under the 'OpenEVSE' tab in the web gui). | The activated device should have it's max current set to the 'Max Current total' value while the other device should be set to the 'Max safe Current' value.
| 3 | Both devices are awake | 1. Power on both devices <br/> 2. Activate both devices | Both devices should have their max current set to the 'Max safe Current' value |
| 4 | One device is activated while the other device is offline | 1. Power on one device while the other is powered off or disconnected from the MQTT broker <br/> 2. Activate the connected device | The activated device should display a message saying: "Please wait..." while trying to communicate with the other device for ten seconds. The device should then disable itself and display an error message before going back to sleep.