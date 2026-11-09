#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "invalidation_region.h"
#include "prescribed_values_view.h"

namespace celeris {
    /*
        Контекст связывает динамический вычислитель со статическими компонентами.
        prescribed_values() — неизменный в течение вычисления источник D.
        resolve(t) возвращает f_(E,D)(t) через тот же адаптер и ту же версию D/E:
        заданное значение, попадание кэша или вложенное вычисление.
        Контекст не предоставляет кэш и операций изменения исходных данных.
        Время жизни заимствованного контекста ограничено вычислением; сохранять
        его/ссылки на него для последующих запросов нельзя. Исключения resolve
        передаются вызывающему коду; они не означают успешное получение Y.
    */
    template<std::copy_constructible Y>
    class TrajectoryEvaluationContext {
    public:
        virtual ~TrajectoryEvaluationContext() = default;

        [[nodiscard]]
        virtual const PrescribedValuesViewInterface<Y>& prescribed_values() const noexcept = 0;

        [[nodiscard]]
        virtual Y resolve(simulation::Timestamp timestamp) = 0;
    };

    /*
        Вычислитель задаёт семейство f_(E,D): Timestamp -> Y. Конфигурация E
        фиксирована; смена E/его параметров требует отдельного протокола адаптера.
        Совпадение Y — смысловое, operator== не требуется.

        compatible(D) задаёт ЯВНОЕ отношение Q(D,E), определено для любого D,
        стабильно при фиксированных данных, не меняет их, не вызывает resolve.
        Зависит от логического D, не от типа/устройства представления источника.
        Заимствованное представление не сохраняется для будущих вычислений.
        По умолчанию принимаются все источники, включая пустой. Дополнительные
        предусловия запрещено скрывать за true: они должны входить в Q.

        evaluate(t, context), при Q(D,E) и t, не заданном непосредственно в D:
        - возвращает f_(E,D)(t); заданные точки удовлетворяют f(t)=D(t), а адаптер
          обслуживает их непосредственно, без evaluate;
        - результат зависит только от E,D,t и значений разрешённых зависимостей,
          не от истории запросов, наличия кэша или порядка вычислений;
        - локальная работа и ВСЯ цепочка resolve завершаются; одного отсутствия
          прямого самовызова недостаточно. Ранги необязательны;
        - поддерживает вложенные вызовы того же объекта В ОДНОМ потоке. Состояние
          отдельного вызова не повреждается вложенными вычислениями;
        - не изменяет D/определяемую функцию. Вспомогательное состояние допустимо
          только при сохранении этих законов, в том числе после исключения.

        Область времени — весь Timestamp. Forward-only реализация обязана явно
        определить продолжение, например начальным состоянием до t0. Ограничение
        области времени молча не вводится. Политика сетки/опор — часть E.

        invalidation_region(D, change) при Q(D,E) и Q(D',E), где D'=change(D):
        возвращает НЕПУСТОЙ УКАЗАТЕЛЬ на область I. Пустое множество допустимо.
        Для каждого t вне I обязательно f_(E,D)(t)=f_(E,D')(t). Область может быть
        шире реального влияния, включая весь Timestamp; учитываются транзитивные
        зависимости и непосредственное изменение заданного значения.
        Описание change не изменяет источник. Регион используется до изменения D;
        его заимствования должны оставаться действительными до конца удаления.
        Дополнительный интервальный интерфейс заявляет точное описание I.

        Операции завершаются, технические исключения допускаются. Неуспешное
        вычисление не публикует частичный результат; корректные результаты
        завершившихся вложенных запросов могут остаться в кэше. При исключении
        обязательства относительно той же версии данных сохраняются.

        Использование последовательное: внешние изменения/замена E запрещены
        во время запросов. Потокобезопасность не обещана. Сигнатуры/NVI не
        доказывают эти смысловые требования; контрактные тесты проверяют примеры.
    */
    template<std::copy_constructible Y>
    class TrajectoryEvaluatorInterface {
    public:
        using Timestamp = simulation::Timestamp;

        virtual ~TrajectoryEvaluatorInterface() = default;

        [[nodiscard]]
        bool compatible(const PrescribedValuesViewInterface<Y>& values) const {
            return compatible_impl(values);
        }

        [[nodiscard]]
        Y evaluate(Timestamp timestamp, TrajectoryEvaluationContext<Y>& context) {
            return evaluate_impl(timestamp, context);
        }

        [[nodiscard]]
        std::unique_ptr<InvalidationRegionInterface> invalidation_region(
            const PrescribedValuesViewInterface<Y>& values,
            const PrescribedValuesChange<Y>& change) const
        {
            auto region = invalidation_region_impl(values, change);

            if (!region) {
                throw std::logic_error("Evaluator returned a null invalidation region");
            }

            return region;
        }

    private:
        virtual bool compatible_impl(const PrescribedValuesViewInterface<Y>&) const {
            return true;
        }

        virtual Y evaluate_impl(Timestamp timestamp, TrajectoryEvaluationContext<Y>& context) = 0;

        virtual std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<Y>& values,
            const PrescribedValuesChange<Y>& change) const = 0;
    };

    /*
        НЕОБЯЗАТЕЛЬНЫЙ усиленный контракт. rank(t,D) — натуральный ограниченный
        снизу ранг, стабильный при E,D,t, определён для совместимого D.
        Каждый resolve(s) из evaluate(t) к НЕЗАДАННОЙ точке требует rank(s)<rank(t),
        независимо от попадания кэша. Заданные точки завершаются непосредственно.
        На минимальном ранге нет дальнейших незаданных зависимостей.
        Локальное вычисление всё равно обязано завершаться: проверка рангов
        контролирует рекурсию, но не произвольный цикл внутри evaluate.
        Базовый интерфейс не требует рангов; dynamic_cast обнаруживает только
        заявленную возможность, а не доказывает правдивость этих обещаний.
    */
    template<std::copy_constructible Y>
    class RankedTrajectoryEvaluatorInterface : public TrajectoryEvaluatorInterface<Y> {
    public:
        using Rank = std::uint64_t;

        [[nodiscard]]
        Rank rank(
            simulation::Timestamp timestamp,
            const PrescribedValuesViewInterface<Y>& values) const
        {
            return rank_impl(timestamp, values);
        }

    private:
        virtual Rank rank_impl(
            simulation::Timestamp timestamp,
            const PrescribedValuesViewInterface<Y>& values) const = 0;
    };
}
