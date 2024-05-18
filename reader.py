try:
    import serial
except ImportError:
    env.Execute("$PYTHONEXE -m pip install pyserial")
    import serial

try:
    import threading
except ImportError:
    env.Execute("$PYTHONEXE -m pip install threading")
    import threading

try:
    import time
except ImportError:
    env.Execute("$PYTHONEXE -m pip install time")
    import time

try:
    import tkinter as tk
except ImportError:
    env.Execute("$PYTHONEXE -m pip install tk")
    import tkinter as tk

try:
    from tkinter import ttk
except ImportError:
    env.Execute("$PYTHONEXE -m pip install tkinter")
#    from tkinter import ttk




import tkinter as tk
from tkinter import ttk
import serial
import threading
import time

verbose = True
line = ""
reading_on = True
# Función para iniciar o detener la lectura del puerto serie
def toggle_serial_reading():
    global reading_on, serial_thread, serial_button, serial_reading
    global serial_reading
    if serial_reading:
        serial_reading = False
        serial_button.config(text="Reanudar")
        reading_on = False
    else:
        serial_reading = True
        serial_button.config(text="Detener")
        reading_on = True

lcd_latest_counter=0
lcd_latest_two = {}
s01_latest_two = {}
s01_latest_counter=0

s01_tx_msg=[]
lcd_tx_msg = {}
s01_rx_msg=[]
lcd_rx_msg = {}
firsttime_lcd=True
firsttime_s01=True
def safe_list_get (l, idx, default):
  try:
    return l[idx]
  except IndexError:
    return default
  
# Función para decodificar y mostrar el mensaje
def decode_and_display(line):
    global lcd_messages, s01_messages, lcd_latest_counter, lcd_latest_two, s01_latest_counter, s01_latest_two, s01_latest_counter
    global lcd_tx_msg, s01_tx_msg, lcd_rx_msg, s01_rx_msg, firsttime_lcd, firsttime_s01     
    #print ("decoding: "+line)
    # Decodificar la línea
    parts = line.split("::|")
    sender = parts[0]
    if(sender==""):
        #print ("Error decoding: "+line)
        # throw error
        raise Exception("Error decoding: "+line)
        #return
    #print ("sender: "+sender)
    
    try:
        params = parts[1].strip().split("mSrc")
        #print ("params 0: "+str(params))
        params = "mSrc"+params[1]
        # replace | with , to split the string
        params = params.replace("|", ",")
        params = params.strip().split(", ")
        #print ("params: "+str(params))
        params_dict = {}
        for param in params:
            key, value = param.split(":")
            params_dict[key.strip()] = value.strip()
    except:
        if sender == "LCD":
            lcd_display.insert(tk.END, "\n"+line+ "\n")
            lcd_display.see("end") 
        elif sender == "S01":
            s01_display.insert(tk.END, "\n"+line+ "\n")
            s01_display.see("end")
        return False

    # Añadir mensaje a la lista correspondiente
    if sender == "LCD":
        lcd_messages.append(params_dict)
        #lcd_display.insert(tk.END, format_message(params_dict)+ "\n")
        #lcd_display.see("end")

        params_dict.pop('id', None)
        #params_dict.pop('ts', None)
        params_dict.pop('cTemp', None)

        equal_msg = True

        if(params_dict.get('mSrc')=="32" and params_dict.get('mDst')=="1"):
            if(lcd_tx_msg!=params_dict):
                equal_msg = False
                lcd_tx_msg = params_dict
            else:
                lcd_display.insert(tk.END, ">")
        elif(params_dict.get('mSrc')=="0" and params_dict.get('mDst')=="32"):
            if(lcd_rx_msg!=params_dict):
                equal_msg = False
                lcd_rx_msg = params_dict
            else:
                lcd_display.insert(tk.END, "<")
        #else:
        #    lcd_display.insert(tk.END, "\n"+format_message(params_dict)+ "\n")
        #    lcd_display.see("end")

        if(equal_msg == False):
            if(firsttime_lcd):
                firsttime_lcd = False
            else:
                lcd_display.insert(tk.END, "\n")
            lcd_display.insert(tk.END, format_message(lcd_tx_msg)+ "\n")
            lcd_display.insert(tk.END, format_message(lcd_rx_msg)+ "\n")
            lcd_display.see("end")

                
    elif sender == "S01":
        s01_messages.append(params_dict)
        #s01_display.insert(tk.END, format_message(params_dict)+ "\n")
        #s01_display.see("end")

        params_dict.pop('id', None)
        #params_dict.pop('ts', None)
        params_dict.pop('cTemp', None)
        equal_msg = True

        if(params_dict.get('mSrc')=="32" and params_dict.get('mDst')=="1"):
            if(safe_list_get(s01_tx_msg,0,0)!=params_dict):
                equal_msg = False
                s01_tx_msg.insert(0,params_dict)
            else:
                s01_display.insert(tk.END, ">")
        elif(params_dict.get('mSrc')=="0" and params_dict.get('mDst')=="32"):
            if(safe_list_get(s01_rx_msg,0,0)!=params_dict):
                equal_msg = False
                s01_rx_msg.insert(0,params_dict)
            else:
                s01_display.insert(tk.END, "<")
        #else:
        #    s01_display.insert(tk.END, "\n"+format_message(params_dict)+ "\n")
        #    s01_display.see("end")

        if(equal_msg == False):
            if(firsttime_s01):
                firsttime_s01 = False
            else:
                s01_display.insert(tk.END, "\n")
                #if(safe_list_get(s01_tx_msg,0,0)==0):
                s01_display.insert(tk.END, format_message(safe_list_get(s01_tx_msg,0,0))+ "\n")
                    #s01_tx_msg.insert(0,0)

                #if(safe_list_get(s01_rx_msg,0,0)==0):
                s01_display.insert(tk.END, format_message(safe_list_get(s01_rx_msg,0,0))+ "\n")
                    #s01_rx_msg.insert(0,0)
            s01_display.see("end")
    if(sender == "LCD" or sender == "S01"):
        return True
    return False


# Función para formatear un mensaje
def format_message(msg):
    background_color = "white"  # Color de fondo predeterminado

    # Verificar los valores de mSrc y mDst
    if msg.get('mSrc') == '31' and msg.get('mDst') == '1':
        background_color = "gray"
    elif msg.get('mSrc') == '0' and msg.get('mDst') == '32':
        background_color = "light blue"

    return "| {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} | {:>3} |{:>5}|".format(
        msg.get('mSrc', ''), msg.get('mDst', ''), msg.get('onOff', ''), msg.get('mType', ''),
        msg.get('temp', ''), msg.get('cTemp', ''), msg.get('acmode', ''), msg.get('fanmode', ''),
        msg.get('write', ''), msg.get('login', ''), msg.get('unk', ''), msg.get('cP', ''),
        msg.get('uMgc', ''), msg.get('acError', ''), msg.get('id', ''),
        background_color=background_color
    )


# Función para leer mensajes del puerto serie en un hilo separado
def read_serial():
    global ser 
    line = ""
    while True:
        try:
            if reading_on == False:
                continue
            #print (ser.readline().decode("utf-8"))
            line = ser.readline().decode("utf-8").strip()
            if line == "":
                continue
            if((not decode_and_display(line)) or verbose==True):
                print("- "+line)
            #print (line)
        except Exception as e:
            #if(verbose):
            print("$ "+line)
            pass

# Configurar la ventana
root = tk.Tk()
root.title("Puerto Serie COM2")
root.geometry("1000x800")
#root.attributes('-fullscreen', True) 
root.state('zoomed')

# Crear marcos para los mensajes LCD y S01
lcd_frame = ttk.Frame(root)
lcd_frame.pack(side=tk.LEFT, expand=True, fill=tk.BOTH)
s01_frame = ttk.Frame(root)
s01_frame.pack(side=tk.RIGHT, expand=True, fill=tk.BOTH)




## Crear etiquetas para los encabezados de parámetros para S01
#s01_params_labels = [ttk.Label(s01_frame, text=param) for param in params_header]
#for label in s01_params_labels:
#    label.pack(side=tk.TOP)

# Crear etiquetas para los encabezados
lcd_label = ttk.Label(lcd_frame, text="LCD", font=("Consolas", 20, "bold"))
lcd_label.pack(side=tk.TOP)
s01_label = ttk.Label(s01_frame, text="S01", font=("Consolas", 20, "bold"))
s01_label.pack(side=tk.TOP)

# Crear etiquetas para los encabezados de parámetros
params_header = [" mSrc", " mDst", "onOff", "mType", " temp", "cTemp", "acmod", "fanmo", "write",
                 "login", "  unk", "   cP", " uMgc", "Error", "   id"]

# create a string with the header
header = "|"
for param in params_header:
    header += "{}|".format(param)
# create a label with the header
header_label = ttk.Label(lcd_frame, text=header, justify='left',font=("Consolas", 14, "bold"))
header_label.pack(side=tk.TOP, anchor='w')

header_label = ttk.Label(s01_frame, text=header, justify='left',font=("Consolas", 14, "bold"))
header_label.pack(side=tk.TOP, anchor='w')
header_label = ttk.Label(lcd_frame, text=header, justify='left',font=("Consolas", 14, "bold"))
header_label.pack(side=tk.BOTTOM, anchor='w')

header_label = ttk.Label(s01_frame, text=header, justify='left',font=("Consolas", 14, "bold"))
header_label.pack(side=tk.BOTTOM, anchor='w')


# Crear área de texto para los mensajes LCD
lcd_display = tk.Text(lcd_frame, wrap=tk.WORD, bg="darkblue", fg="lightgrey", font=("Consolas", 14, "bold"))
#lcd_display.insert(tk.END, "Mensajes LCD:", ('header',))
lcd_display.tag_config('header', justify='center')
lcd_display.pack(side=tk.LEFT, expand=True, fill=tk.BOTH)

# Crear área de texto para los mensajes S01
s01_display = tk.Text(s01_frame, wrap=tk.WORD, bg="darkblue", fg="lightgrey", font=("Consolas", 14, "bold"))
#s01_display.insert(tk.END, "Mensajes S01:", ('header',))
s01_display.tag_config('header', justify='center')

s01_display.pack(side=tk.RIGHT, expand=True, fill=tk.BOTH)

# Crear botón para iniciar o detener la lectura del puerto serie
serial_button = ttk.Button(root, text="Detener", command=toggle_serial_reading)
serial_button.pack(side=tk.TOP)
serial_reading = False

# Inicializar listas para almacenar los mensajes
lcd_messages = []
s01_messages = []

failed = True

while failed:
    try:
        # Abrir puerto serie COM3 a 115200 baudios
        ser = serial.Serial('COM3', 921600, timeout=1)
        failed = False
    except serial.SerialExcept:
        print("No se pudo abrir el puerto serie COM3. Esperando 1s...")
        time.sleep(1)
        failed = True


# Crear un hilo para leer los mensajes del puerto serie
serial_thread = threading.Thread(target=read_serial)
serial_thread.daemon = True
serial_thread.start()

root.mainloop()

