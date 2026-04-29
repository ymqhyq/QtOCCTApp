#pragma once
#include <ActData_UserParameter.h>
#include <ActData_Common.h>
#include <ActData_ParameterDTO.h>
#include <gp_Pnt.hxx>

DEFINE_STANDARD_HANDLE(ActData_PointParameterDTO, ActData_ParameterDTO)

//! \ingroup AD_DF
//!
//! Data Transfer Object (DTO) corresponding to data wrapped with
//! Real Array Parameter without any OCAF connectivity.
class ActData_PointParameterDTO : public ActData_ParameterDTO
{
public:

	// OCCT RTTI
	DEFINE_STANDARD_RTTI_INLINE(ActData_PointParameterDTO, ActData_ParameterDTO)

public:

	//! Constructor accepting GID.
	//! \param theGID [in] GID.
	ActData_PointParameterDTO(const ActAPI_ParameterGID& theGID) : 
		ActData_ParameterDTO(theGID, Parameter_LASTFREE),m_point(0,0,0) {}

public:

	gp_Pnt m_point; //!< Actual array.

};

//-----------------------------------------------------------------------------
// Parameter
//-----------------------------------------------------------------------------

DEFINE_STANDARD_HANDLE(ActData_PointParameter, ActData_UserParameter)

//! \ingroup AD_DF
//!
//! Node Parameter representing an array of real values.
class ActData_PointParameter : public ActData_UserParameter
{
public:

	// OCCT RTTI
	DEFINE_STANDARD_RTTI_INLINE(ActData_PointParameter, ActData_UserParameter)

public:

	ActData_EXPORT static Handle(ActData_PointParameter) Instance();

public:

	ActData_EXPORT void
		SetValue(const gp_Pnt theVal,
			const ActAPI_ModificationType theModType = MT_Touched,
			const Standard_Boolean doResetValidity = Standard_True,
			const Standard_Boolean doResetPending = Standard_True);

	ActData_EXPORT gp_Pnt GetValue();

protected:

	ActData_PointParameter();

private:

	virtual Standard_Boolean isWellFormed() const;
	virtual Standard_Integer parameterType() const;

private:

	virtual void
		setFromDTO(const Handle(ActData_ParameterDTO)& theDTO,
			const ActAPI_ModificationType theModType = MT_Touched,
			const Standard_Boolean doResetValidity = Standard_True,
			const Standard_Boolean doResetPending = Standard_True);

	virtual Handle(ActData_ParameterDTO)
		createDTO(const ActAPI_ParameterGID& theGID);

protected:

	//! Tags for the underlying CAF Labels.
	enum Datum
	{
		DS_X = ActData_UserParameter::DS_DatumLast,
		DS_Y,
		DS_Z,
		DS_DatumLast = DS_X + RESERVED_DATUM_RANGE
	};

};



