#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <3ds.h>

#ifndef CTRQ_DEFAULT_POST_PUT_BUFFER_SIZE
#define CTRQ_DEFAULT_POST_PUT_BUFFER_SIZE (2 * 1024 * 1024)
#endif

#ifndef CTRQ_USER_AGENT
#define CTRQ_USER_AGENT "ctrq/0.0.1"
#endif

#ifndef CTRQ_HEADER_BUFFER_SIZE
#define CTRQ_HEADER_BUFFER_SIZE 4 * 1024
#endif

#ifndef CTRQ_DOWNLOAD_BUFFER_SIZE
#define CTRQ_DOWNLOAD_BUFFER_SIZE 0x1000
#endif

namespace ctrq {

enum httpc_failure {
	NONE = 0,
	OPEN_CONTEXT,
	DISABLE_SSL_VERIFY,
	SET_KEEP_ALIVE,
	SET_KEEP_ALIVE_HEADER,
	SET_USER_AGENT,
	SET_HEADER,
	BEGIN_REQUEST,
	GET_RESPONSE_STATUS_CODE,
	ADD_RAW_POST_DATA,
	ADD_ASCII_POST_PARAM
};

class response {
private:
	bool http_context_closed = false;

	std::vector<u8> body;
	bool body_populated = false;

	std::string body_string;
	bool body_string_populated = false;

public:
	u32 status = 0;
	Result result = 0;
	httpc_failure failure = NONE;

	httpcContext context;

	~response() {
		close_context();
	}

	/**
	 * @brief Returns whether the request has failed.
	 * 
	 * @return true The request failed at some point.
	 * @return false The request completed successfully.
	 */
	bool has_failed() { return R_FAILED(result); }

	/**
	 * @brief Gets a header from the response.
	 * 
	 * @param header The header key.
	 * @return std::string Its value.
	 */
	std::string get_header(const std::string& header) {
		char buffer[CTRQ_HEADER_BUFFER_SIZE] = {0};
		Result result;

		if (http_context_closed)
			return std::string();

		result = httpcGetResponseHeader(&context, header.c_str(), buffer,
			sizeof(buffer));

		return std::string(buffer);
	}

	/**
	 * @brief Gets the response's body as a byte vector.
	 * 
	 * @return std::vector<u8> The response's body.
	 */
	std::vector<u8>& get_body() {
		u8 buffer[CTRQ_DOWNLOAD_BUFFER_SIZE] = {0};
		u32 downloaded_size = 0;

		if (body_populated || http_context_closed)
			return body;

		do {
			result = httpcDownloadData(&context, buffer, sizeof(buffer),
				&downloaded_size);
			std::copy(buffer, buffer + downloaded_size, std::back_inserter(body));
		} while(result == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);

		body_populated = true;
		return body;
	}

	std::string& get_body_string() {
		if (http_context_closed || body_string_populated)
			return body_string;

		auto body = get_body();
		body_string = std::string(reinterpret_cast<char*>(body.data()), body.size());

		return body_string;
	}

	/**
	 * @brief Closes the HTTP context associated to this response.
	 * 
	 * The context is also closed when destructing the response object.
	 */
	void close_context() {
		if (!http_context_closed)
			httpcCloseContext(&context);
	}
};

/**
 * @brief Initializes the http context.
 * 
 * You **must not** call this function if you already initialized the HTTP
 * context yourself.
 * 
 * @param buffer_size The POST/PUT buffer size.
 */
Result initialize(int buffer_size = CTRQ_DEFAULT_POST_PUT_BUFFER_SIZE) {
	return httpcInit(buffer_size);
}

/**
 * @brief Terminates the HTTP context.
 * 
 * Calling this function is not nece
 */
void terminate() {
	httpcExit();
}

Result add_headers(httpcContext* context,
	const std::map<std::string, std::string>* headers) {
	Result result;

	if (headers == nullptr)
		return 0;

	for (auto it = headers->begin(); it != headers->end(); it++) {
		result = httpcAddRequestHeaderField(context, it->first.c_str(),
			it->second.c_str());
		if (R_FAILED(result))
			return result;
	}
	return 0;
}

#define _CTRQ_FALSE_IF_FAILED(fail, eval) \
	if (R_FAILED(result = (eval))) {\
		res.failure = (fail);\
		res.result = result;\
		return false;\
	}

bool perform_request(response& res) {
	Result result;

	_CTRQ_FALSE_IF_FAILED(
		BEGIN_REQUEST,
		httpcBeginRequest(&res.context)
	)
	_CTRQ_FALSE_IF_FAILED(
		GET_RESPONSE_STATUS_CODE,
		httpcGetResponseStatusCode(&res.context, &res.status)
	)

	return true;
}

bool setup_context(response& res, const std::string& url,
	HTTPC_RequestMethod method,
	const std::map<std::string, std::string>* headers, int default_proxy,
	bool disable_ssl_verification, bool keep_alive) {
	Result result;

	_CTRQ_FALSE_IF_FAILED(
		OPEN_CONTEXT,
		httpcOpenContext(&res.context, HTTPC_METHOD_GET, url.c_str(), 
			default_proxy)
	)

	if (disable_ssl_verification) {
		_CTRQ_FALSE_IF_FAILED(
			DISABLE_SSL_VERIFY,
			httpcSetSSLOpt(&res.context, SSLCOPT_DisableVerify)
		)
	}

	_CTRQ_FALSE_IF_FAILED(
		SET_USER_AGENT,
		httpcAddRequestHeaderField(&res.context, "User-Agent", CTRQ_USER_AGENT)
	)

	_CTRQ_FALSE_IF_FAILED(
		SET_HEADER,
		add_headers(&res.context, headers)
	)

	_CTRQ_FALSE_IF_FAILED(
		SET_KEEP_ALIVE,
		httpcSetKeepAlive(&res.context, keep_alive ? HTTPC_KEEPALIVE_ENABLED :
			HTTPC_KEEPALIVE_DISABLED)
	)

	if (keep_alive) {
		_CTRQ_FALSE_IF_FAILED(
			SET_KEEP_ALIVE_HEADER,
			httpcAddRequestHeaderField(&res.context, "Connection", "Keep-Alive")
		)
	}

	return true;
}

/**
 * @brief Performs a GET request.
 * 
 * @param url The target url.
 * @param headers An optional map of header values.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification (default = true).
 * @param keep_alive Whether to support keep-alive. (default true)
 * @return response The request's response.
 */
response get(const std::string& url,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_GET, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	perform_request(res);
	return res;
}

bool add_raw_post_data(response& res, const u32* data, u32 length) {
	if (R_FAILED(res.result = httpcAddPostDataRaw(&res.context, data, length))) {
		res.failure = ADD_RAW_POST_DATA;
		return false;
	}
	return true;
}

/**
 * @brief Performs a POST request.
 * 
 * Performs a post request with a raw binary body.
 * 
 * @param url The target url.
 * @param body The binary body.
 * @param length The body's length.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response post(const std::string& url, const u8* body, size_t length,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_POST, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	if (!add_raw_post_data(res, reinterpret_cast<const u32*>(body), length))
		return res;

	perform_request(res);
	return res;
}

/**
 * @brief Performs a POST request.
 * 
 * Performs a POST request with a binary body (from a vector).
 * 
 * @param url The target url.
 * @param body The binary body.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response post(const std::string& url, const std::vector<u8>& body,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	return post(url, body.data(), body.size(), headers, default_proxy,
		disable_ssl_verification, keep_alive);
}

/**
 * @brief Performs a POST request.
 * 
 * Performs a POST request with a (preferrably ascii string) as body.
 * 
 * @param url The target url.
 * @param body The string body.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response post(const std::string& url, const std::string& body,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	return post(url, reinterpret_cast<const u8*>(body.data()), body.size(),
		headers, default_proxy, disable_ssl_verification, keep_alive);
}

bool add_ascii_post_param(response& res, const char* field, const char* value) {
	if (R_FAILED(res.result = httpcAddPostDataAscii(&res.context, field, value))) {
		res.failure = ADD_ASCII_POST_PARAM;
		return false;
	}
	return true;
}

bool add_ascii_post_params(response& res, const std::map<std::string, std::string>& params) {
	for (auto it = params.begin(); it != params.end(); it++) {
		if (!add_ascii_post_param(res, it->first.c_str(), it->second.c_str()))
			return false;
	}
	return true;
}

/**
 * @brief Performs a POST request.
 * 
 * Performs a post request using a string map as (ascii) POST parameters.
 * 
 * @param url The target URL.
 * @param params The POST parameters.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response post(const std::string& url,
	const std::map<std::string, std::string>& params,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_POST, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	if (!add_ascii_post_params(res, params))
		return res;

	perform_request(res);
	return res;
}

/**
 * @brief Performs a PUT request.
 * 
 * Performs a PUT request with a raw body.
 * 
 * @param url The target url.
 * @param body The binary body.
 * @param length The body's length.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response put(const std::string& url, const u8* body, size_t length,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_PUT, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	if (!add_raw_post_data(res, reinterpret_cast<const u32*>(body), length))
		return res;

	perform_request(res);
	return res;
}

/**
 * @brief Performs a PUT request.
 * 
 * Performs a raw binary PUT request.
 * 
 * @param url The target url.
 * @param body The body as a byte vector.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response put(const std::string& url, const std::vector<u8>& body,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	return put(url, body.data(), body.size(), headers, default_proxy,
		disable_ssl_verification, keep_alive);
}

/**
 * @brief Performs a PUT request.
 * 
 * Performs a PUT request with a string body.
 * 
 * @param url The target url.
 * @param body The (ascii) string body.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response put(const std::string& url, const std::string& body,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	return put(url, reinterpret_cast<const u8*>(body.data()), body.size(),
		headers, default_proxy, disable_ssl_verification, keep_alive);
}

/**
 * @brief Performs a PUT request.
 * 
 * Performs a PUT request with a key-value map as key-value parameters.
 * 
 * @param url The target url.
 * @param params The key-value params map.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response put(const std::string& url,
	const std::map<std::string, std::string>& params,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_PUT, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	if (!add_ascii_post_params(res, params))
		return res;

	perform_request(res);
	return res;
}

/**
 * @brief Performs a DELETE request.
 * 
 * @param url The target url.
 * @param headers An optional headers map.
 * @param default_proxy Which proxy to use (0 for default).
 * @param disable_ssl_verification Whether to disable SSL verification
 * (default = true).
 * @param keep_alive Whether to activate Keep-alive.
 * @return response The response.
 */
response deleet(const std::string& url,
	const std::map<std::string, std::string>* headers = nullptr,
	int default_proxy = 0, bool disable_ssl_verification = true,
	bool keep_alive = true) {
	response res;

	if (!setup_context(res, url, HTTPC_METHOD_DELETE, headers, default_proxy,
		disable_ssl_verification, keep_alive))
		return res;

	perform_request(res);
	return res;
}

};