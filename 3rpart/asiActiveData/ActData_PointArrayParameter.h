#pragma once
#include <ActData_UserParameter.h>
#include <ActData_Common.h>
#include <ActData_ParameterDTO.h>


DEFINE_STANDARD_HANDLE(ActData_PointArrayDTO, ActData_ParameterDTO)

//! \ingroup AD_DF
//!
//! Data Transfer Object (DTO) corresponding to data wrapped with
//! Real Array Parameter without any OCAF connectivity.
class ActData_PointArrayDTO : public ActData_ParameterDTO
{
public:

	// OCCT RTTI
	DEFINE_STANDARD_RTTI_INLINE(ActData_PointArrayDTO, ActData_ParameterDTO)

public:

	//! Constructor accepting GID.
	//! \param theGID [in] GID.
	ActData_PointArrayDTO(const ActAPI_ParameterGID& theGID) : ActData_ParameterDTO(theGID, Parmeter_PointArray) {}

public:

	Handle(HPointList) Array; //!< Actual array.

};


DEFINE_STANDARD_HANDLE(ActData_PointArrayParameter, ActData_UserParameter)

//! \ingroup AD_DF
//!
//! Node Parameter representing an array of real values.
class ActData_PointArrayParameter : public ActData_UserParameter
{
public:

	// OCCT RTTI
	DEFINE_STANDARD_RTTI_INLINE(ActData_PointArrayParameter, ActData_UserParameter)

public:

	ActData_EXPORT static Handle(ActData_PointArrayParameter) Instance();

public:

	ActData_EXPORT void
		InitEmpty();

	ActData_EXPORT void
		BackupArray();

	ActData_EXPORT void
		SetElement(const Standard_Integer theIndex,
			const gp_XYZ& theValue,
			const ActAPI_ModificationType theModType = MT_Silent,
			const Standard_Boolean doResetValidity = Standard_False,
			const Standard_Boolean doResetPending = Standard_False);

	ActData_EXPORT gp_XYZ
		GetElement(const Standard_Integer theIndex);

	ActData_EXPORT void	SetArray(const Handle(HPointList)& theArray,
			const ActAPI_ModificationType theModType = MT_Touched,
			const Standard_Boolean doResetValidity = Standard_True,
			const Standard_Boolean doResetPending = Standard_True);

	ActData_EXPORT Handle(HPointList) GetArray();

	ActData_EXPORT Standard_Integer NbElements();

protected:

	ActData_PointArrayParameter();

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
		DS_Array_X = ActData_UserParameter::DS_DatumLast,
		DS_Array_Y,
		DS_Array_Z,
		DS_ElemNum,
		DS_DatumLast = DS_Array_X + RESERVED_DATUM_RANGE
	};

};


