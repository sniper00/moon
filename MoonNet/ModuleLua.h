#pragma  once
#include "MacroDefine.h"
#include "Module.h"

class ModuleLua:public moon::Module
{
public:
	ModuleLua();

	~ModuleLua();
	
	bool								Init(const std::string& config) override;

	void								OnEnter()  override;

	void								Update(uint32_t interval)  override;

	void								OnExit() override;

	void								OnMessage(moon::Message*) override;
private:
	struct ModuleLuaImp;
	std::unique_ptr<ModuleLuaImp> m_ModuleLuaImp;
};