/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "aes.h"
#include <vector>
#include <cassert>

/*
char key[] ="1234567890123456";
char iv[]  = "1234567890123456";

aes_helper ah;
ah.set_key(key, iv);

//std::string p1 = string_to_utf8("首先在您的示例中只是 ha hah ha");
std::string p1 = ("首先在您的示例中只是 ha hah ha");

auto ret = ah.encrypt_cbc((uint8_t*)p1.data(), p1.size() + 1, aes_helper::PaddingMode::Zeros);

auto tmp = ah.decrypt_cbc(ret.data(), ret.size(), aes_helper::PaddingMode::Zeros);

std::string str(tmp.begin(), tmp.end());
//printf("%s", utf8_to_string(str).c_str());
printf("%s",(str).c_str());

*/


class aes_helper
{
public:
	static const uint8_t key_size = 16;
	aes_helper()
	{
		memset(m_key, 0, key_size);
		memset(m_iv, 0, key_size);
	}

	enum class PaddingMode
	{
		Zeros,
		PKCS7Padding
		//PKCS5Padding
	};

	//16字节(128位)密钥,16字节(128位)iv, 如果iv为空， 则iv和密钥相同
	void set_key(const char* szkey, const char* sziv = nullptr)
	{
		for (int i = 0; i < key_size; i++)
		{
			m_key[i] = szkey[i];
			if (sziv == nullptr)
			{
				m_iv[i] = m_key[i];
			}
		}

		if (sziv != nullptr)
		{
			for (int i = 0; i < key_size; i++)
			{
				m_iv[i] = sziv[i];
			}
		}
	}
	
	std::vector<uint8_t> encrypt_cbc(uint8_t* inData, size_t inLen,PaddingMode Mode)
	{
		assert(inData != nullptr && inLen > 0);

		size_t len = 0;
		uint8_t padding_value = 0;
		check_padding(inLen, Mode, len, padding_value);
		memcpy(m_buffer.data(), inData, inLen);

		std::vector<uint8_t> ret(len, 0);
		AES128_CBC_encrypt_buffer(ret.data(),m_buffer.data(), len, m_key, m_iv);
		return std::move(ret);
	}

	std::vector<uint8_t> decrypt_cbc(uint8_t* inData, size_t inLen,PaddingMode Mode)
	{
		assert(inData != nullptr && inLen > 0);

		std::vector<uint8_t> ret(inLen, 0);

		AES128_CBC_decrypt_buffer(ret.data(), inData, inLen, m_key, m_iv);

		remove_padding(Mode, ret);
		return ret;
	}

	std::vector<uint8_t> encrypt_ecb(uint8_t* inData, size_t inLen, PaddingMode Mode)
	{
		assert(inData != nullptr && inLen > 0);

		size_t len = 0;
		uint8_t padding_value = 0;
		check_padding(inLen, Mode, len, padding_value);
		memcpy(m_buffer.data(), inData, inLen);

		std::vector<uint8_t> ret(len, 0);
		for (size_t i = 0; i < m_buffer.size(); i += key_size)
		{
			AES128_ECB_encrypt(m_buffer.data() + i, m_key, ret.data() + i);
		}
		return ret;
	}

	std::vector<uint8_t> decrypt_ecb(uint8_t* inData, size_t inLen, PaddingMode Mode)
	{
		assert(inData != nullptr && inLen > 0);

		std::vector<uint8_t> ret(inLen, 0);
		for (size_t i = 0; i < inLen; i += key_size)
		{
			AES128_ECB_decrypt(inData+i, m_key, ret.data()+i);
		}
		
		remove_padding(Mode, ret);
		return ret;
	}

private:
	void remove_padding(PaddingMode Mode, std::vector<uint8_t>& ret)
	{
		if (Mode == PaddingMode::PKCS7Padding)
		{
			uint8_t n = ret.back();
			for (uint8_t i = 0; i < n; i++)
			{
				ret.pop_back();
			}
		}
	}

	void check_padding(size_t inLen, PaddingMode Mode, size_t& len, uint8_t& padding_value)
	{
		len = inLen;
		padding_value = 0;
		switch (Mode)
		{
		case PaddingMode::Zeros:
			if (inLen % 16 == 0)
			{
				len += 16;
			}
			else
			{
				len += (16 - inLen % 16);
			}
			break;
		case PaddingMode::PKCS7Padding:
		{
			if (inLen % 16 == 0)
			{
				len += 16;
				padding_value = 16;
			}
			else
			{
				len += (16 - inLen % 16);
				padding_value = 16 - inLen % 16;
			}
		}
		break;
		default:
			assert(0 && "unknown PaddingMode");
		}

		if (m_buffer.size() < len)
		{
			m_buffer.resize(len);
		}
		memset(m_buffer.data(), padding_value, m_buffer.size());
	}

private:
	uint8_t			m_key[key_size];
	uint8_t			m_iv[key_size];

	std::vector<uint8_t>  m_buffer;
};
