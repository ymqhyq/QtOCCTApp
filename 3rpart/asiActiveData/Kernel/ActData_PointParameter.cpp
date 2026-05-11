#include "pch.h"
#include "ActData_PointParameter.h"
#include "ActData_Utils.h"
#include "TDataStd_Real.hxx"

Handle(ActData_PointParameter) ActData_PointParameter::Instance()
{
	return new ActData_PointParameter();
}

void ActData_PointParameter::SetValue(const gp_Pnt theVal, const ActAPI_ModificationType theModType /*= MT_Touched*/, const Standard_Boolean doResetValidity /*= Standard_True*/, const Standard_Boolean doResetPending /*= Standard_True*/)
{
	if (this->IsDetached())
		throw Standard_ProgramError("Cannot access detached data");

	ActData_Utils::SetRealValue(m_label, DS_X, theVal.X());
	ActData_Utils::SetRealValue(m_label, DS_Y, theVal.Y());
	ActData_Utils::SetRealValue(m_label, DS_Z, theVal.Z());

	// Mark root label of the Parameter as modified (Touched, Impacted or Silent)
	SPRING_INTO_FUNCTION(theModType)
		// Reset Parameter's validity flag if requested
		RESET_VALIDITY(doResetValidity)
		// Reset Parameter's PENDING property
		RESET_PENDING(doResetPending);
}

gp_Pnt ActData_PointParameter::GetValue()
{
	if (!this->IsWellFormed())
		throw Standard_ProgramError("Data inconsistent");

	Standard_Real vX, vY, vZ;
	ActData_Utils::GetRealValue(m_label, DS_X, vX);
	ActData_Utils::GetRealValue(m_label, DS_Y, vY);
	ActData_Utils::GetRealValue(m_label, DS_Z, vZ);
	return gp_Pnt(vX, vY, vZ);
}

ActData_PointParameter::ActData_PointParameter():
	ActData_UserParameter()
{

}

Standard_Boolean ActData_PointParameter::isWellFormed() const
{
	if (!ActData_Utils::CheckLabelAttr(m_label, DS_X,
		TDataStd_Real::GetID()))
		return Standard_False;
	if (!ActData_Utils::CheckLabelAttr(m_label, DS_Y,
		TDataStd_Real::GetID()))
		return Standard_False;
	if (!ActData_Utils::CheckLabelAttr(m_label, DS_Y,
		TDataStd_Real::GetID()))
		return Standard_False;
	return Standard_True;
}

Standard_Integer ActData_PointParameter::parameterType() const
{
	return Parameter_Point;
}

void ActData_PointParameter::setFromDTO(const Handle(ActData_ParameterDTO)& theDTO, const ActAPI_ModificationType theModType /*= MT_Touched*/, const Standard_Boolean doResetValidity /*= Standard_True*/, const Standard_Boolean doResetPending /*= Standard_True*/)
{
	Handle(ActData_PointParameterDTO) MyDTO = Handle(ActData_PointParameterDTO)::DownCast(theDTO);
	this->SetValue(MyDTO->m_point, theModType, doResetValidity, doResetPending);
}

Handle(ActData_ParameterDTO) ActData_PointParameter::createDTO(const ActAPI_ParameterGID& theGID)
{
	Handle(ActData_PointParameterDTO) aRes = new ActData_PointParameterDTO(theGID);
	aRes->m_point = this->GetValue();
	return aRes;
}

