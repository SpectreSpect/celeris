#pragma once

#include <concepts>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>

#include "clock.h"

namespace celeris {
    template<class Y>
    struct SetPrescribedValue {
        simulation::Timestamp timestamp;
        Y value;
    };

    struct ErasePrescribedValue {
        simulation::Timestamp timestamp;
    };

    // Описание ОДНОЙ предполагаемой операции, ещё не изменяющее источник.
    template<class Y>
    using PrescribedValuesChange = std::variant<SetPrescribedValue<Y>, ErasePrescribedValue>;

    /*
        Источник — конечное частичное отображение D: Timestamp -> Y, с уникальными
        ключами и копируемыми значениями. Пустое состояние допустимо. Записи
        сохраняются до явной замены/удаления; самовольного вытеснения нет.

        find(t) возвращает копию заданного значения либо nullopt. Обход const
        источника даёт ВСЕ пары ровно по одному разу, в строго возрастающем
        порядке времени. Ключ и значение доступны только через const-ссылки.
        lower_bound(t) — первая пара с key>=t; upper_bound(t) — с key>t;
        отсутствие обозначается end(). Это позиции в том же const-диапазоне.

        set(t, y) создаёт/заменяет ровно эту запись; остальные пары неизменны.
        erase(t) удаляет ровно этот ключ; отсутствие ключа допустимо.
        После исключения set/erase логическое содержимое ПОЛНОСТЬЮ прежнее.
        Чтение не меняет D. Заимствованные ссылки/итераторы действительны до
        следующего вызова set/erase (включая неуспешный) или уничтожения источника.
        Возвращаемые копии не дают изменять сохранённые логические значения через
        общие mutable-алиасы.

        bidirectional common range выбран явно: он позволяет получить опору
        слева через ranges::prev(upper_bound(t)) без полного обхода. Сложность поиска,
        устройство хранения, default-конструктор, copyability самого источника,
        operator==/default-конструктор у Y и многопоточность не требуются.

        Источник не задаёт влияние точек на остальные времена и не инвалидирует
        кэш. В композиции исключительное право изменения передаётся адаптеру;
        внешний код пользуется его управляемыми операциями. Это предусловие
        композиции, а не зависимость источника от типа адаптера.

        Совместимость конкретного D с вычислителем — отдельное Q(D,E).
        Концепт проверяет выражения/типы, а не перечисленные смысловые законы.
    */
    template<class Source, class Y>
    concept prescribed_values_source =
        std::copy_constructible<Y> &&
        std::ranges::bidirectional_range<const Source> &&
        std::ranges::common_range<const Source> &&
        requires(
            Source& source,
            const Source& readable,
            simulation::Timestamp timestamp,
            Y value)
        {
            { readable.find(timestamp) } -> std::same_as<std::optional<Y>>;
            { readable.lower_bound(timestamp) } ->
                std::same_as<std::ranges::iterator_t<const Source>>;
            { readable.upper_bound(timestamp) } ->
                std::same_as<std::ranges::iterator_t<const Source>>;
            { (*std::ranges::begin(readable)).first } -> std::same_as<const simulation::Timestamp&>;
            { (*std::ranges::begin(readable)).second } -> std::same_as<const Y&>;
            { source.set(timestamp, std::move(value)) } -> std::same_as<void>;
            { source.erase(timestamp) } -> std::same_as<void>;
        };

    template<class Y, prescribed_values_source<Y> Source>
    void apply_prescribed_values_change(Source& source, const PrescribedValuesChange<Y>& change) {
        std::visit(
            [&](const auto& operation) {
                auto timestamp = operation.timestamp;

                if constexpr (requires { operation.value; }) {
                    source.set(timestamp, Y(operation.value));
                } else {
                    source.erase(timestamp);
                }
            },
            change
        );
    }
}
