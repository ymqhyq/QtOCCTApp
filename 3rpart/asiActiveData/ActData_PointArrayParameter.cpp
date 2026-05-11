#include "ActData_PointArrayParameter.h"
#include "ActData_Utils.h"
#include <Standard_ProgramError.hxx>
#include <TDataStd_Integer.hxx>


Handle(ActData_PointArrayParameter) ActData_PointArrayParameter::Instance()
{
	return new ActData_PointArrayParameter();
}

void ActData_PointArrayParameter::InitEmpty()
{
	// Clean up array
	ActData_Utils::ChooseLabelByTag(m_label, DS_Array_X, Standard_True).ForgetAllAttributes();
	ActData_Utils::ChooseLabelByTag(m_label, DS_Array_Y, Standard_True).ForgetAllAttributes();
	ActData_Utils::ChooseLabelByTag(m_label, DS_Array_Z, Standard_True).ForgetAllAttributes();

	// Store Array dimension only
	ActData_Utils::SetIntegerValue(m_label, DS_ElemNum, 0);
}

void ActData_PointArrayParameter::BackupArray()
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");

	ActData_Utils::BackupRealArray(m_label, DS_Array_X);
	ActData_Utils::BackupRealArray(m_label, DS_Array_Y);
	ActData_Utils::BackupRealArray(m_label, DS_Array_Z);
}

void ActData_PointArrayParameter::SetElement(const Standard_Integer theIndex, 
	const gp_XYZ& theValue, const ActAPI_ModificationType theModType /*= MT_Silent*/, 
	const Standard_Boolean doResetValidity /*= Standard_False*/, 
	const Standard_Boolean doResetPending /*= Standard_False*/)
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");

	if (theIndex > this->NbElements())
		throw Standard_Failure("SetElement -- out of range");
	ActData_Utils::SetRealArrayElem(m_label, DS_Array_X, theIndex, theValue.X());
	ActData_Utils::SetRealArrayElem(m_label, DS_Array_Y, theIndex, theValue.Y());
	ActData_Utils::SetRealArrayElem(m_label, DS_Array_Z, theIndex, theValue.Z());

	// Mark root label of the Parameter as modified (Touched, Impacted or Silent)
	SPRING_INTO_FUNCTION(theModType)
		// Reset Parameter's validity flag if requested
		RESET_VALIDITY(doResetValidity)
		// Reset Parameter's PENDING property
		RESET_PENDING(doResetPending);
}

gp_XYZ ActData_PointArrayParameter::GetElement(const Standard_Integer theIndex)
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");

	if (theIndex > this->NbElements())
		throw Standard_Failure("GetElement -- out of range");

	return gp_XYZ(ActData_Utils::GetRealArrayElem(m_label, DS_Array_X, theIndex),
		ActData_Utils::GetRealArrayElem(m_label, DS_Array_Y, theIndex),
		ActData_Utils::GetRealArrayElem(m_label, DS_Array_Z, theIndex));
}

void ActData_PointArrayParameter::SetArray(const Handle(HPointList)& theArray, const ActAPI_ModificationType theModType /*= MT_Touched*/, const Standard_Boolean doResetValidity /*= Standard_True*/, const Standard_Boolean doResetPending /*= Standard_True*/)
{
	if (this->IsDetached())
		throw Standard_ProgramError("Cannot access detached data");

	if (theArray.IsNull())
		this->InitEmpty();
	else
	{
		Handle(HRealArray) xArr = new HRealArray(theArray->Lower(), theArray->Upper());
		Handle(HRealArray) yArr = new HRealArray(theArray->Lower(), theArray->Upper());
		Handle(HRealArray) zArr = new HRealArray(theArray->Lower(), theArray->Upper());
		for (Standard_Integer i = theArray->Lower(); i <= theArray->Upper(); i++)
		{
			xArr->SetValue(i, theArray->Value(i).X());
			yArr->SetValue(i, theArray->Value(i).Y());
			zArr->SetValue(i, theArray->Value(i).Z());
		}
		// Store Array data
		ActData_Utils::InitRealArray(m_label, DS_Array_X, xArr);
		ActData_Utils::InitRealArray(m_label, DS_Array_Y, yArr);
		ActData_Utils::InitRealArray(m_label, DS_Array_Z, zArr);

		// Store Array dimension
		ActData_Utils::SetIntegerValue(m_label, DS_ElemNum, theArray->Length());
	}
}


// 修复方法：实现 GetArray 方法体，而不是在类外部重新声明函数签名
Handle(HPointList) ActData_PointArrayParameter::GetArray()
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");
	PointList pList;
	Standard_Integer nElems = this->NbElements();
	if (nElems == 0)
		return new HPointList(pList); // 空数组

	Handle(HRealArray) xArr = ActData_Utils::GetRealArray(m_label, DS_Array_X);
	Handle(HRealArray) yArr = ActData_Utils::GetRealArray(m_label, DS_Array_Y);
	Handle(HRealArray) zArr = ActData_Utils::GetRealArray(m_label, DS_Array_Z);
	if (xArr.IsNull() || yArr.IsNull() || zArr.IsNull())
		return new HPointList(pList);// HPointList(0, 0); // 空数组

	Standard_Integer lower = xArr->Lower();
	Standard_Integer upper = xArr->Upper();
	
	for (Standard_Integer i = lower; i < upper; ++i)
	{
		pList.Append(gp_XYZ(xArr->Value(i), yArr->Value(i), zArr->Value(i)));
	}
	return new HPointList(pList);
}

Standard_Integer ActData_PointArrayParameter::NbElements()
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");

	Standard_Integer aValue;
	ActData_Utils::GetIntegerValue(m_label, DS_ElemNum, aValue);
	return aValue;
}

ActData_PointArrayParameter::ActData_PointArrayParameter()
	:ActData_UserParameter()
{

}

Standard_Boolean ActData_PointArrayParameter::isWellFormed() const
{
	if (!ActData_Utils::CheckLabelAttr(m_label, DS_ElemNum,
		TDataStd_Integer::GetID()))
		return Standard_False;
	return Standard_True;
}

Standard_Integer ActData_PointArrayParameter::parameterType() const
{
	return Parameter_RealArray;
}

void ActData_PointArrayParameter::setFromDTO(const Handle(ActData_ParameterDTO)& theDTO, const ActAPI_ModificationType theModType /*= MT_Touched*/, const Standard_Boolean doResetValidity /*= Standard_True*/, const Standard_Boolean doResetPending /*= Standard_True*/)
{
	Handle(ActData_PointArrayDTO) MyDTO = Handle(ActData_PointArrayDTO)::DownCast(theDTO);
	this->SetArray(MyDTO->Array, theModType, doResetValidity, doResetPending);
}

Handle(ActData_ParameterDTO) ActData_PointArrayParameter::createDTO(const ActAPI_ParameterGID& theGID)
{
	Handle(ActData_PointArrayDTO) aRes = new ActData_PointArrayDTO(theGID);
	aRes->Array = this->GetArray();
	return aRes;
}

