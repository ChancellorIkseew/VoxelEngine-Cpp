# Консоль

Для работы с командным интерпретатором предоставляется библиотека **console**.

## Создание команд

Для создания команды консоли используется следующая функция:

```python
console.add_command(схема: str, исполнитель: function)
```

Схема имеет следующий синтаксис:

```
название позиционные параметры {именованные параметры}
```

Название может содержать:
- латинницу
- цифры (кроме первого символа) 
- `.`, `_`, `-`

Позиционные параметры разделяются пробелами и имеют следующий синтаксис:

```
название:тип                                    (вариант 1)
название:тип=по-умолчанию                       (вариант 2)
название:тип~центральное-значение               (вариант 3)
название:тип=по-умолчанию~центральное-значение  (вариант 4)
```

Доступные типы:
- **int** - целое число
- **num** - дробное число
- **str** - строка
- **sel** - селектор (id объекта представленный целым числом)
- **enum** - перечисление

На вариантах 3 и 4 показан оператор `~` позволяющий использовать относительные значения. *Центральное значение* - значение, относительно которого будет указываться пользовательское. Например позиция игрока.

Относительный оператор работает только с числами (num или int)

В качестве центральных значений могут указываться переменные, назначаемые через **console.set**.

Пример:

```python
x:num~pos.x
```

Переменные можно указывать и в качестве значений по-умолчанию, при использовании префикса `$`:

```python
t:int=$time
```

Перечисления указывазываются в формате:

```python
mode:[replace|destruct|none]
```

Либо через переменную:

```python
mode:enum $modes
```

Селекторы указываются с префиксом `@`. На данный момент являются заглушкой, по причине отсутствия объектной модели. Следует делать опциональными и использовать переменные:

```python
obj:sel=$obj.id # obj.id - id игрока
```

Именованные аргументы указываются в специальном блоке, ограниченном фигурными скобками `{ }` по той же схеме.

Пример:

```python
eval name:str="World" {greeting:str='Hello'}
```

## Примеры схем команд

Схемы стандартных команд можно найти в файле `res/script/stdcmd.lua`.

Пример - команда **tp**:

```python
tp obj:sel=$obj.id x:num~pos.x y:num~pos.y z:num~pos.z
```

Полный lua код создания команды:

```lua
console.add_command(
    "tp obj:sel=$obj.id x:num~pos.x y:num~pos.y z:num~pos.z",
    "Teleport object",
    function (args, kwargs)
        player.set_pos(unpack(args))
    end
)
```

- В args передаются готовые значения позиционных аргументов.
- В kwargs передается таблица значений именованных аргументов.

Проверку и приведение типов интерпретатор команд производит автоматически.