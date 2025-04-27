import folium
import pandas as pd

current_lat = -22.712868
current_long = -47.629333

# Criar o mapa centralizado na sua coordenada
mapa = folium.Map(location=[current_lat, current_long], zoom_start=14)

##############################################
################ Pontos ESP32 ################
##############################################
df = pd.read_csv('coord_esp32.csv')

timestamp_index = 0
latitude_index = 2
longitude_index = 3

# Adicionar os pontos ao mapa
for _, row in df.iterrows():
    folium.CircleMarker(
        location=[row[latitude_index], row[longitude_index]],
        radius=1,
        color='blue',
        fill=True,
        fill_color='blue',
        fill_opacity=0.7,
        popup=f"Timestamp: {row[timestamp_index]}"
    ).add_to(mapa)




# Salvar o mapa
mapa.save('mapa_pontos.html')
print("Mapa salvo como 'mapa_pontos.html'")