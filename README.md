# Desafio-K2-Avancado

Desafio para implementação de sensor biométrico com sensor cardiaco

# Como funciona?

Após o registro da digital com o sensor biométrico R307, começa a leitura do sensor cardíaco AD8232 e envia esses dados para um servidor Flash, que serão processados via interface gráfica Streamlit. O sistema continua funcionando até que a digital do paciente seja inserida novamente no sensor biométrico, interrompendo a leitura de dados do eletrocardiograma.
