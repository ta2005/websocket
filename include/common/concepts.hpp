#ifndef WS_CONCEPTS_HPP
#define WS_CONCEPTS_HPP


namespace ws {

    template<typename T>
    concept Awaitable = requires(T t){
	{t.operator co_await() };
    };
    template<typename T>
    concept isAsyncSocket = requires(T t){
	{t.read()} -> Awaitable;
    };

} // namespace ws

#endif
