/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once

namespace moon
{
	template<typename T>
	class BinaryWriter
	{
	public:
		using TStreamPointer = T*;
		using StreamType = typename T::StreamType;

		explicit BinaryWriter(TStreamPointer t)
			:m_ms(t->ToStream())
		{
		}

		BinaryWriter(const BinaryWriter& t) = delete;
		BinaryWriter& operator=(const BinaryWriter& t) = delete;

		template<typename TData>
		void WriteFront(TData& t)
		{
			m_ms.WriteFront(&t, 0, 1);
		}

		template<typename TData>
		void WriteFront(TData* t,size_t count)
		{
			m_ms.WriteFront(t, 0, count);
		}

		template<typename TData>
		void Write(const TData& t)
		{
			WriteImp(t);
		}

	
		template<typename TData>
		void WriteVector(std::vector<TData> t)
		{
			static_assert(std::is_pod<TData>::value, "type T must be pod.");
			Write(t.size());
			for (auto& it : t)
			{
				write(it);
			}
		}

		template<typename TData>
		void WriteArray(TData* t, size_t count)
		{
			static_assert(std::is_pod<TData>::value, "type T must be pod.");
			m_ms.WriteBack(t, 0, count);
		}

		template<class TData>
		BinaryWriter& operator<<(TData&& value)
		{
			Write(value);
			return *this;
		}

		size_t Size()
		{
			return m_ms.Size();
		}

	private:
		template<typename TData>
		void WriteImp(const TData & t)
		{
			static_assert(std::is_pod<TData>::value, "type T must be pod.");
			m_ms.WriteBack(&t, 0, 1);
		}

		void WriteImp(const std::string & str)
		{
			m_ms.WriteBack(str.data(), 0, str.size() + 1);
		}
	private:
		StreamType&			m_ms;
	};



};




