#pragma once
#include <vector>
#include "Value.hpp"

class DataColumn
{
public:
	DataColumn(const std::string& name, DataType Type)
		:m_Name(name)
		, m_Type(Type)
	{

	}

	DataColumn(const DataColumn& other)
		:m_Name(other.m_Name), m_Type(other.m_Type)
	{

	}

	DataColumn(DataColumn&& other)
		:m_Name(std::move(other.m_Name)), m_Type(other.m_Type)
	{
		other.m_Type = DataType::null;
	}

	const std::string& name() const
	{
		return m_Name;
	}

	const DataType& type() const
	{
		return m_Type;
	}
private:
	std::string m_Name;
	DataType m_Type;
};

class DataColumns
{
public:
	DataColumns()
	{
	}

	DataColumns(const DataColumns& other)
		:m_Columns(other.m_Columns)
	{
	}

	DataColumns(DataColumns&& other)
		:m_Columns(std::move(other.m_Columns))
	{
	}

	DataColumn& Add(const std::string& name, DataType type)
	{
		m_Columns.emplace_back(DataColumn(name, type));
		return m_Columns.back();
	}

	void Clear()
	{
		m_Columns.clear();
	}

	DataColumn& operator[](size_t col)
	{
		if (col >= m_Columns.size())
		{
			throw std::out_of_range("col index out of range.");
		}
		return m_Columns[col];
	}

	size_t Size()
	{
		return m_Columns.size();
	}

private:
	std::vector<DataColumn> m_Columns;
};

class DataRow
{
public:
	friend class DataRows;

	DataRow()
		:m_Cols(nullptr)
	{

	}

	DataValue& operator[](size_t nCol)
	{
		assert(m_Cols != nullptr);
		if (nCol >= Datas.size())
		{
			throw std::out_of_range("col index out of range.");
		}
		return Datas[nCol];
	}

	const DataValue& operator[](size_t nCol) const
	{
		if (nCol >= Datas.size())
		{
			throw std::out_of_range("col index out of range.");
		}
		return Datas[nCol];
	}

	void Clear()
	{
		Datas.clear();
	}

	void Resize(size_t nCol)
	{
		Datas.resize(nCol);
	}

	DataRow(const DataRow& other)
		:Datas(other.Datas),m_Cols(other.m_Cols)
	{

	}

	DataRow(DataRow&& other)
		:Datas(std::move(other.Datas)), m_Cols(other.m_Cols)
	{

	}

protected:
	void SetColumns(DataColumns* col) { m_Cols = col; }

private:
	std::vector<DataValue> Datas;
	DataColumns*			       m_Cols;
};

class DataRows
{
public:
	friend class DataTable;

	DataRows()
		:m_Cols(nullptr)
	{

	}

	DataRows(const DataRows& other)
		:m_Rows(other.m_Rows), m_Cols(other.m_Cols)
	{

	}

	DataRows(DataRows&& other)
		:m_Rows(std::move(other.m_Rows)), m_Cols(other.m_Cols)
	{

	}

	DataRow& operator[](size_t nRow)
	{
		if (nRow >= m_Rows.size())
		{
			throw std::out_of_range("row index out of range.");
		}
		return m_Rows[nRow];
	}

	DataRow& Add()
	{
		m_Rows.emplace_back(DataRow());
		m_Rows.back().SetColumns(m_Cols);
		return m_Rows.back();
	}

	void Clear()
	{
		for (auto& r : m_Rows)
		{
			r.Clear();
		}
	}

	void Resize(size_t nRow, size_t nCol)
	{
		m_Rows.clear();
		m_Rows.resize(nRow);
		for (auto& r : m_Rows)
		{
			r.Clear();
			r.Resize(nCol);
		}
	}

	size_t Size()
	{
		return m_Rows.size();
	}

protected:
	void SetColumns(DataColumns* col)
	{
		m_Cols = col;
		for (auto& it : m_Rows)
		{
			it.SetColumns(m_Cols);
		}
	}
private:
	std::vector<DataRow>      m_Rows;
	DataColumns*				      m_Cols;
};

class DataTable
{
public:
	DataTable()
	{
		Rows.SetColumns(&Columns);
	}

	~DataTable()
	{

	}

	DataTable(const DataTable& other)
		:Columns(other.Columns),Rows(other.Rows)
	{
		Rows.SetColumns(&Columns);
	}

	DataTable(DataTable&& other)
		: Columns(std::move(other.Columns)),Rows(std::move(other.Rows))
	{
		Rows.SetColumns(&Columns);
	}

	void Clear()
	{
		Columns.Clear();
		Rows.Clear();
	}

	DataColumns  Columns;
	DataRows	    Rows;

	const std::string& name() { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

private:
	std::string  m_name;
};

