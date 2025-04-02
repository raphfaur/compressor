#pragma once
#include<memory>
#include<iostream>

template<typename T>
class Transformer {
protected:
    std::shared_ptr<std::basic_istream<T> > istream;
    std::shared_ptr<std::basic_ostream<T>> ostream = std::shared_ptr<std::basic_ostream<T>>(&std::cout, [](auto _){});
public:
    void set_output(std::shared_ptr<std::basic_ostream<T>> ostream);
    Transformer(std::shared_ptr<std::basic_istream<T> > s) : istream(s){};
    Transformer() = default;

    virtual void run() = 0;
};

template<typename T>
void Transformer<T>::set_output(std::shared_ptr<std::basic_ostream<T>> o) {
    ostream = o;
}