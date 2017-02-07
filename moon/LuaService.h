#pragma  once
#include "service.h"

class LuaService:public moon::service
{
public:
	LuaService();

	~LuaService();
	
	bool	 init(const std::string& config) override;
			 
	void	 start()  override;
			 
	void	 update()  override;
			 
	void	 destory() override;
			 
	void	 on_message(moon::message* msg) override;
			 
	void	 error(const std::string& msg);

private:
	struct Imp;
	std::unique_ptr<Imp> imp_;
};