# GLSL Hash Table Templates

Краткое описание набора шаблонов для реализации хеш-таблицы на GLSL с помощью `#define` и пользовательского препроцессора (`#include`, `#pragma once`).

## Что нужно определить перед подключением

Ниже перечислены макросы и сущности, которые ожидают шаблоны.

### Обязательные

- `HT_PREFIX`  
  Префикс для всех сгенерированных имён. Используется через:
  ```glsl
  #define HASH_TABLE_PREFIX(NAME) PR(HT_PREFIX, NAME)
  ```
  Пример:
  ```glsl
  #define HT_PREFIX my_hash_table_
  ```

- `HASH_TABLE_SIZE`  
  Размер таблицы.

- `SLOT_KEY_TYPE`  
  Тип ключа.

- `SLOT_VALUE_TYPE`  
  Тип значения.

- `KEY_HASH_FUNC(key)`  
  Макрос или функция хеширования ключа. Должен возвращать `uint`.

- `SLOTS_BUFFER`  
  Буфер со слотами таблицы.

- `TABLE_COUNTERS`  
  Структура/экземпляр счётчиков типа `HashTableCounters`.

- `INVALID_ID`  
  Идентификатор невалидного слота.

### Что должно быть в слоте

Элементы `SLOTS_BUFFER[...]` должны содержать как минимум поля:

```glsl
key
value
state
```

Поле `state` использует следующие состояния из `hash_table_decl.glsl`:

- `SLOT_EMPTY`
- `SLOT_LOCKED`
- `SLOT_TOMB`
- `SLOT_OCCUPIED`

## Опциональные макросы

- `KEY_COMP_FUNC(a, b)`  
  Пользовательское сравнение ключей. Если не задан, используется `a == b`.

- `VALUE_INIT_FUNC(key, success)`  
  Инициализация значения при создании нового слота внутри `get_or_create_slot`.  
  Должен вернуть `SLOT_VALUE_TYPE` и выставить `success`.

- `INIT_SLOT_CALLBACK(slot_id, key, value)`  
  Колбэк после инициализации нового слота.

- `TOMB_CHECK_LIST_SIZE`  
  Размер локальной очереди tomb-слотов для переиспользования. По умолчанию:
  ```glsl
  32u
  ```

## Что определяют сами шаблоны

### `hash_table_decl.glsl`

Объявляет:

- `HASH_TABLE_PREFIX(NAME)`
- константы состояний слота
- `MAX_PROBES`
- `COUNT_TABLE_COUNTERS`
- `struct HashTableCounters`

### `hash_table_common_impl.glsl`

Добавляет общую служебную логику:

- tomb-очередь для переиспользования удалённых слотов
- функции работы со счётчиками пустых / занятых / tomb-слотов
- функции `push_tomb_id` и `pop_tail_tomb_id`

### `hash_table_get_or_create_impl.glsl`

Добавляет функцию получения или создания слота по ключу.

### `hash_table_lookup_remove_impl.glsl`

Добавляет функции поиска, удаления и вставки значения без инициализатора.

## Сгенерированные функции

Все имена ниже реально будут иметь префикс `HT_PREFIX`.
Например, при

```glsl
#define HT_PREFIX my_hash_table_
```

функция `HASH_TABLE_PREFIX(get_or_create_slot)` превратится в:

```glsl
my_hash_table_get_or_create_slot
```

### Счётчики

Подключение `hash_table_common_impl.glsl` генерирует:

- `add_empty_counter(uint slot_id, uint add_value)`  
  Увеличивает один из шардированных счётчиков пустых слотов.

- `reduce_read_empty_counter()`  
  Суммирует все шардированные счётчики пустых слотов.

- `add_occupied_counter(uint slot_id, uint add_value)`  
  Увеличивает один из шардированных счётчиков занятых слотов.

- `reduce_read_occupied_counter()`  
  Суммирует все шардированные счётчики занятых слотов.

- `add_tomb_counter(uint slot_id, uint add_value)`  
  Увеличивает один из шардированных счётчиков tomb-слотов.

- `reduce_read_tomb_counter()`  
  Суммирует все шардированные счётчики tomb-слотов.

### Вспомогательные функции tomb-очереди

- `push_tomb_id(uint slot_id)` -> `bool`  
  Добавляет tomb-слот во временную кольцевую очередь для последующего переиспользования.

- `pop_tail_tomb_id()` -> `uint`  
  Возвращает следующий tomb-слот из очереди или `INVALID_ID`, если очередь пуста.

### Основной API таблицы

- `get_or_create_slot(SLOT_KEY_TYPE key, out uint out_slot_id, out bool created)` -> `bool`  
  Ищет слот по ключу. Если слот уже существует, возвращает его `slot_id` и `created = false`.  
  Если не существует, пытается создать новый слот (в том числе с переиспользованием tomb-слота) и возвращает `created = true`.

- `lookup_hash_table_slot_id(SLOT_KEY_TYPE key, bool read_only = false)` -> `uint`  
  Ищет слот по ключу и возвращает его id или `INVALID_ID`, если ключ не найден.  
  При `read_only = true` читает `state` без atomic-операции.

- `remove_slot_from_hash_table(SLOT_KEY_TYPE key)` -> `bool`  
  Помечает найденный слот как `SLOT_TOMB`. Возвращает `true`, если ключ был найден или уже находился в tomb-состоянии.

- `set_slot_value(SLOT_KEY_TYPE key, SLOT_VALUE_TYPE value)` -> `bool`  
  Вставляет новый ключ со значением, если такого ключа ещё нет.  
  Если ключ уже существует, возвращает `false` и не перезаписывает значение.

## Важные замечания

- `get_or_create_slot` и `set_slot_value` используют функции `push_tomb_id` / `pop_tail_tomb_id`, значит для них должен быть подключён `hash_table_common_impl.glsl`.
- `std430`/атомики/барьеры памяти и layout буферов нужно согласовать с C++-стороной вручную.
- `COUNT_TABLE_COUNTERS` в текущем файле объявлен как `16u;` с точкой с запятой внутри `#define`. Это работает только пока макрос используется в безопасных местах, но вообще лучше убрать `;` из макроса.
- В `hash_table_lookup_remove_impl.glsl` есть место с `atomicAdd(TABLE_COUNTERS.count_tomb, 1u);`, хотя в `HashTableCounters` `count_tomb` — это массив. Если это не специальная обёртка, тут, похоже, нужна индексация по счётчику, как в `hash_table_counters_impl.glsl`.

## Пример набора определений

```glsl
#define HT_PREFIX my_hash_table_
#define HASH_TABLE_SIZE 1024u
#define SLOT_KEY_TYPE uvec2
#define SLOT_VALUE_TYPE uint
#define KEY_HASH_FUNC(KEY) my_hash_uvec2(KEY)
#define KEY_COMP_FUNC(A, B) ((A) == (B))
```

После этого можно подключать нужные реализации:

```glsl
#include "hash_table_common_impl.glsl"
#include "hash_table_get_or_create_impl.glsl"
#include "hash_table_lookup_remove_impl.glsl"
```
