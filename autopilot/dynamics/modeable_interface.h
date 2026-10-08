#pragma once

namespace celeris {
    template<class ModeType>
    class ModeableInterface {
    public:
        virtual void set_mode(ModeType mode) = 0;
        virtual const ModeType& get_mode() const = 0;
    };
}
