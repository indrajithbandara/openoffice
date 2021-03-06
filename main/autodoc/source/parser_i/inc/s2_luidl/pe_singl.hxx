/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef LUIDL_PE_SINGL_HXX
#define LUIDL_PE_SINGL_HXX



// USED SERVICES
	// BASE CLASSES
#include <s2_luidl/parsenv2.hxx>
#include <s2_luidl/pestate.hxx>
	// COMPONENTS
	// PARAMETERS

namespace ary
{
    namespace idl
    {
        class Singleton;
        class SglIfcSingleton;
    }
}


namespace csi
{
namespace uidl
{

class PE_Type;


class PE_Singleton : public UnoIDL_PE,
				     public ParseEnvState
{
  public:
						PE_Singleton();
	virtual				~PE_Singleton();

	virtual void	 	EstablishContacts(
							UnoIDL_PE *			io_pParentPE,
							ary::Repository &	io_rRepository,
							TokenProcessing_Result &
												o_rResult );
	virtual void	  	ProcessToken(
							const Token &		i_rToken );

	virtual void		Process_MetaType(
							const TokMetaType &	i_rToken );
	virtual void		Process_Identifier(
							const TokIdentifier &
												i_rToken );
	virtual void		Process_Punctuation(
							const TokPunctuation &
												i_rToken );
	virtual void		Process_Default();

  private:
	enum E_State
	{
		e_none = 0,
		need_name,
		need_curlbr_open,
		e_std,
		in_service,
		need_finish,
		in_base_interface,
		e_STATES_MAX
	};


#if 0
	enum E_TokenType	/// @ATTENTION  Do not change existing values (except of tt_MAX) !!! Else array-indices will break.
	{
		tt_metatype = 0,
		tt_identifier = 1,
		tt_punctuation = 2,
		tt_startoftype = 3,
		tt_MAX
	};
	typedef void (PE_Singleton::*F_TOK)(const char *);


	void				On_need_singleton_MetaType(const char * i_sText);
	void				On_need_name_Identifer(const char * i_sText);
	void				On_need_curlbr_open_Punctuation(const char * i_sText);
	void				On_std_GotoService(const char * i_sText);
	void				On_std_Punctuation(const char * i_sText);
	void				On_need_finish_Punctuation(const char * i_sText);

	void				CallHandler(
							const char *		i_sTokenText,
							E_TokenType			i_eTokenType );
#endif // 0

	void				On_Default();

	virtual void		InitData();
	virtual void		TransferData();
	virtual void		ReceiveData();
	virtual UnoIDL_PE &	MyPE();

	// DATA
//	static F_TOK		aDispatcher[e_STATES_MAX][tt_MAX];

	E_State				eState;
	String              sData_Name;
	bool				bIsPreDeclaration;
	ary::idl::Singleton *
	            		pCurSingleton;
	ary::idl::SglIfcSingleton *
	            		pCurSiSingleton;

	Dyn<PE_Type>		pPE_Type;
	ary::idl::Type_id	nCurParsed_Type;
};


}   // namespace uidl
}   // namespace csi



#endif

