#CIS 310 Final Project
#Ciaran Grabowski, Hailey Gumbs, Yahya Daoud
#12/15/2024

import asyncio
import customtkinter
import serial
from bleak import BleakScanner, BleakClient

import threading
import queue

#BLE UUIDs
SERVICE_UUID = "00000000-5EC4-4083-81CD-A10B8D5CF6EC"
TEMP_CHAR_UUID = "00000001-5EC4-4083-81CD-A10B8D5CF6EC"

DEVICE_NAME = "NANO-BLE"

#Info for Serial reading
PORT = "COM3"
BAUDRATE = 9600

#Color dictionary
colors = {
    0: "Red",
    1: "Yellow",
    2: "Green",
    3: "Cyan",
    4: "Blue",
    5: "Magenta",
}

#Configure event loop for asyncio
def configure_event_loop():
    try:
        # Use the default Windows event loop policy if available
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    except AttributeError:
        pass

#Connect to BLE Device
async def connect_to_device(temperature_queue):
    print("Scanning for BLE devices...")
    devices = await BleakScanner.discover()

    target_device = None
    for device in devices:
        print(f"Found device: {device.name}, Address: {device.address}")
        if device.name == DEVICE_NAME:
            target_device = device
            break

    if not target_device:
        print(f"Device '{DEVICE_NAME}' not found. Make sure it's advertising and try again.")
        return

    print(f"Connecting to {DEVICE_NAME} at address {target_device.address}...")
    async with BleakClient(target_device.address) as client:
        print(f"Connected to {DEVICE_NAME}!")

        print("Discovering services and characteristics...")
        for service in client.services:
            print(f"Service: {service.uuid}")
            if service.uuid.lower() == SERVICE_UUID.lower():
                for char in service.characteristics:
                    print(f"  Characteristic: {char.uuid} - Properties: {char.properties}")
                    if char.uuid.lower() == TEMP_CHAR_UUID.lower():
                        print("Subscribing to temperature notifications...")

                        def handle_temperature_data(sender, data):
                            temperature_data = data.decode("utf-8")
                            temperature = float(temperature_data)
                            temperature_queue.put(temperature)

                        await client.start_notify(char.uuid, handle_temperature_data)
                        print("Receiving temperature data... Press Ctrl+C to exit.\n\n")
                        
                        try:
                            while True:
                                await asyncio.sleep(1)
                        except KeyboardInterrupt:
                            print("\nStopping notifications...")
                            await client.stop_notify(char.uuid)
                            print("Disconnected.")
                            return

        print("Temperature service or characteristic not found on the device.")

#Read incoming data from the arduino Serial monitor
#in order to read the color of the LED
def read_serial_data(loop, data_queue):
    asyncio.set_event_loop(loop)  # Set the event loop
    try:
        with serial.Serial(PORT, BAUDRATE, timeout=1) as ser:
            print(f"Connected to {PORT} at {BAUDRATE} baud.")
            while True:
                line = ser.readline().decode("utf-8").strip()

                if line == "Entering Display Mode...":
                    print(f"{line}")
                elif line == "Entering Temperature Mode...":
                    print(f"{line}")

                if line.isdigit():
                    key = int(line)
                    if key in colors:
                        data_queue.put(colors[key])
    except serial.SerialException as e:
        print(f"Serial Error: {e}")
    except KeyboardInterrupt:
        print("\nStopped by user.")

#GUI Setup
customtkinter.set_appearance_mode("dark")
customtkinter.set_default_color_theme("green")

# Custom tkinter application
class App(customtkinter.CTk):
    def __init__(self, data_queue, temperature_queue):
        super().__init__()
        self.data_queue = data_queue
        self.temperature_queue = temperature_queue
        self.geometry("600x600")

        self.grid_rowconfigure(0, weight=1)  # configure grid system
        self.grid_columnconfigure(0, weight=1)

        # Slider
        self.slider = customtkinter.CTkSlider(
            self,
            from_=0,
            to=40,
            number_of_steps=100,  # Optional: ensures smooth stepping
        )
        self.slider.set(0)  # Set initial value of the slider
        self.slider.pack(pady=10)  # Add some padding for positioning

        # Temperature Text Box
        self.temperature_label = customtkinter.CTkLabel(
            self,
            text="Temperature: 0.0 °C",
            width=200,
            height=50,
            fg_color="gray",
            corner_radius=10
        )
        self.temperature_label.pack(pady=10)

        # Label
        self.label = customtkinter.CTkLabel(
            self,
            width=300,
            height=300,
            text="",
            fg_color="white",
            corner_radius=90
        )
        self.label.pack(pady=20)
        self.label.place(relx=0.5, rely=0.5, anchor="center")

        # Periodically check the queues
        self.check_queue()
        self.check_temperature_queue()

    #Methods to check the two queues for data from the Serial Monitor and BLE
    def check_queue(self):
        # Color queue
        while not self.data_queue.empty():
            message = self.data_queue.get()
            self.label.configure(fg_color=message)  # Update label with new data
        self.after(100, self.check_queue)  # Check again after 100 ms

    def check_temperature_queue(self):
        # Temperature queue
        while not self.temperature_queue.empty():
            temperature = self.temperature_queue.get()
            self.slider.set(temperature)
            self.temperature_label.configure(text=f"Temperature: {temperature:.1f} °C")
        self.after(100, self.check_temperature_queue)  # Check again after 100 ms

# Run the BLE scanner in a separate thread
def start_ble_task(loop, temperature_queue):
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(connect_to_device(temperature_queue))
    finally:
        loop.close()

#MAIN
if __name__ == "__main__":
    configure_event_loop()

    #Queues used for communication of data across threads
    data_queue = queue.Queue()
    temperature_queue = queue.Queue()

    #Event Loop thread for reading the Serial Monitor
    serial_loop = asyncio.new_event_loop()
    serialData_thread = threading.Thread(target=read_serial_data, args=(serial_loop, data_queue), daemon=True)
    serialData_thread.start()

    #Event Loop for reading from the bluetooth device
    ble_loop = asyncio.new_event_loop()
    ble_thread = threading.Thread(target=start_ble_task, args=(ble_loop, temperature_queue), daemon=True)
    ble_thread.start()

    #Start the GUI
    app = App(data_queue, temperature_queue)
    app.mainloop()

    # Stop the serial and BLE loops after the GUI is closed
    ble_loop.call_soon_threadsafe(ble_loop.stop)
    serial_loop.call_soon_threadsafe(serial_loop.stop)
    ble_thread.join()
    serialData_thread.join()