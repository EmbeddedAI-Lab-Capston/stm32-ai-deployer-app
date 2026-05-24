# STM32 uyumlu LSTM export notlari

ST Edge AI / X-CUBE-AI LSTM modelleri tamamen reddetmez, fakat TFLite
modelin desteklenen operator formunda olmasi gerekir.

## Desteklenen hedef

TFLite graph icinde LSTM katmani mumkunse su operator olarak gorunmelidir:

```text
UNIDIRECTIONAL_SEQUENCE_LSTM
```

Su operatorler STM32 kod uretimi icin desteklenmez:

```text
WHILE
TensorList*
FlexTensorListReserve
FlexTensorListStack
SELECT_TF_OPS
```

Bu operatorleri goruyorsaniz model standart mobil TFLite runtime icin Flex
operatorleriyle export edilmis demektir. Bu model X-CUBE-AI tarafinda C koduna
donusturulemez.

## Keras model onerisi

Basit, stateless ve sabit sekilli bir Keras LSTM kullanin:

```python
inputs = tf.keras.layers.Input(shape=(timesteps, features), batch_size=1)
x = tf.keras.layers.LSTM(
    units,
    activation="tanh",
    recurrent_activation="sigmoid",
    return_sequences=False,
    return_state=False,
    stateful=False,
)(inputs)
outputs = tf.keras.layers.Dense(num_classes, activation="softmax")(x)
model = tf.keras.Model(inputs, outputs)
```

Kacinilacak ayarlar:

```python
return_state=True
stateful=True
time_major=True
tf.while_loop
custom RNN cell
Lambda icinde dongu
```

## TFLite export ornegi

```python
import tensorflow as tf

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.target_spec.supported_ops = [
    tf.lite.OpsSet.TFLITE_BUILTINS,
]
converter._experimental_lower_tensor_list_ops = True

tflite_model = converter.convert()
with open("model_lstm_stm32_float32.tflite", "wb") as f:
    f.write(tflite_model)
```

INT8 export icin:

```python
def representative_dataset():
    for sample in calibration_samples:
        yield [sample.astype("float32")]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter._experimental_lower_tensor_list_ops = True
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()
with open("model_lstm_stm32_int8.tflite", "wb") as f:
    f.write(tflite_model)
```

Export sonrasi Pipeline Wizard modeli once `stedgeai analyze` ile kontrol eder.
Analiz WHILE/FlexTensorList hatasi verirse model yeniden export edilmelidir.

## Alternatif

Elinizde orijinal `.h5` veya `.keras` model varsa, TFLite yerine dogrudan onu
deneyin. ST Edge AI Keras LSTM/GRU icin ayri destek saglar, ancak batch size
1 ve state yonetimi gibi kisitlar vardir.
