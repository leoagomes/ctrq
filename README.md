# ctrq

CTRq is a thin C++11 wrapper around [libctru's](https://github.com/smealum/ctrulib) HTTP request facilities.

CTRq's focus is on simplicity and "correctness" (in the sense of trying my best not to mess up) rather than performance.
Currently it is not completely tested, its interface is far from final and all help/feedback is greatly appreciated.

## Usage

Just add `ctrq.hpp` to your project.

### Documentation

There are Doxygen comments on functions (which should do the trick) and the code should be somewhat clean. In any case,
here's a brief description...

CTRq exposes in the `ctrq` namespace the functions `get`, `post`, `put` and `deleet` (since `delete` is a C++ keyword).
All of these functions take some optional arguments, such as a header map (`std::map<std::string, std::string>*`) and
whether or not to disable SSL verification. `post` and `put` also take a body parameter, which can be of type
`std::string`, `std::vector<u8>`, `const u8*` or `const u32*`. There are other functions in the `ctrq` namespace,
but they shouldn't be used, really.

All request-type functions return a `response` object, which holds the response `status`, internal ctrulib `result`,
a `failure` value (which can be used to tell at which step the request failed, whenever it does) and ctrulib's
`httpcContext`. This class also provides methods for getting the response body (`get_body()` and `get_body_string()`), any response headers (`get_header(std::string& header_name)`) and whether the request `has_failed`.

You **must** `ctrq::initialize()` the context (which you can skip if you have `httpcInit` already in your code) before
performing any requests, and should `ctrq::terminate()` as well (which is the same as `httpcExit()`) before finishing
your app.

## Licensing

This is released under an MIT license. I don't really care what you do with it so long as you agree I'm not
responsible for it. If you feel like you did something cool that deserves a mention in this readme, though, do contact me!

## Contributing

Just open an issue and PR. Please do contribute! I'm currently developing this library based on my specific needs,
but if you happen to want some other functionality, tell me so and I'll try my best to work that out.

Thanks! :-)
