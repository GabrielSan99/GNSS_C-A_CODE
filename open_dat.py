import pandas as pd
import numpy as np
import matplotlib.pyplot as plt 

# Lê o arquivo assumindo que as colunas são separadas por espaços
df = pd.read_csv('CPMG_1p_R1.dat', delim_whitespace=True, header=None)

# Se quiser nomear as colunas:
df.columns = ['Coluna1', 'Coluna2', 'Coluna3']

print(df.head())


plt.figure(figsize=(10, 6))  # Tamanho do gráfico

plt.plot(df['Coluna1'], label='Coluna 1')
plt.plot(df['Coluna2'], label='Coluna 2')
plt.plot(df['Coluna3'], label='Coluna 3')

plt.title('Gráfico das 3 colunas')
plt.xlabel('Índice')
plt.ylabel('Valor')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()