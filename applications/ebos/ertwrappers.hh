/*
  Copyright (C) 2013-2014 by Andreas Lauser
  Copyright (c) 2013 by SINTEF ICT, Applied Mathematics.
  Copyright (c) 2013 by Uni Research AS

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
 * \file
 *
 * \brief This file implements several wrapper classes around the
 *        opaque ERT data types.
 *
 * These classes are shamelessly ripped-off from opm-core and are
 * required to make writing ECL files exception safe...
 */
#ifndef EWOMS_ERT_WRAPPERS_HH
#define EWOMS_ERT_WRAPPERS_HH

#if HAVE_ERT

#include <ert/ecl/fortio.h>
#include <ert/ecl/ecl_endian_flip.h>
#include <ert/ecl/ecl_grid.h>
#include <ert/ecl/ecl_kw_magic.h>
#include <ert/ecl/ecl_kw.h>
#include <ert/ecl/ecl_sum.h>
#include <ert/ecl/ecl_util.h>
#include <ert/ecl/ecl_init_file.h>
#include <ert/ecl/ecl_file.h>
#include <ert/ecl/ecl_rst_file.h>
#include <ert/ecl_well/well_const.h>

#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>

#include <opm/core/utility/ErrorMacros.hpp>
#include <opm/material/Valgrind.hpp>

#include "eclwellmanager.hh"

namespace Ewoms {

// forward definition of the EclGridManager class. We need this for
// specialization...
template <class TypeTag>
class EclGridManager;

/// \cond 0

// required to make the compiler happy if the grid manager is not EclGridManager...

template <class GridManager>
std::string getErtCaseName__(const GridManager &gridManager)
{ OPM_THROW(std::logic_error, "You need to chose the EclGridManager to write ECL files"); }

template <class TypeTag>
std::string getErtCaseName__(const EclGridManager<TypeTag> &gridManager)
{ return gridManager.caseName(); }

template <class GridManager>
const Opm::EclipseGridConstPtr getEclGrid__(const GridManager &gridManager)
{ OPM_THROW(std::logic_error, "You need to chose the EclGridManager to write ECL files"); }

template <class TypeTag>
const Opm::EclipseGridConstPtr getEclGrid__(const EclGridManager<TypeTag> &gridManager)
{ return gridManager.eclGrid(); }

/// \endcond

class ErtBaseKeyword
{
public:
    virtual ~ErtBaseKeyword() {}
};

/*!
 * \ingroup EclBlackOilSimulator
 *
 * \brief This is a smart pointer class for ERT's ecl_kw_type
 *        structure.
 */
template <typename T>
class ErtKeyword : public ErtBaseKeyword
{
public:
#if HAVE_ERT
    typedef ecl_kw_type ErtHandleType;
#else
    typedef int ErtHandleType;
#endif

    // don't allow copies for objects of this class
    ErtKeyword(const ErtKeyword &) = delete;

    // Default constructor
    ErtKeyword()
        : ertHandle_(0)
    {}

    /// Initialization from single-precision array.
    ErtKeyword(const std::string& name,
               const std::vector<float>& data)
        : ertHandle_(0)
    { set(name, data); }

    /// Initialization from double-precision array.
    ErtKeyword(const std::string& name,
               const std::vector<double>& data)
        : ertHandle_(0)
    { set(name, data); }

    /// Initialization from integer array.
    ErtKeyword(const std::string& name,
               const std::vector<int>& data)
        : ertHandle_(0)
    { set(name, data); }

    /// Initialization from string array.
    ErtKeyword(const std::string& name,
               const std::vector<const char*>& data)
        : ertHandle_(0)
    { set(name, data); }

    ~ErtKeyword()
    {
#if HAVE_ERT
        if (ertHandle_)
            ecl_kw_free(ertHandle_);
#endif
    }

    template <class DataElementType>
    void set(const std::string name, const std::vector<DataElementType>& data)
    {
#if HAVE_ERT
        if(ertHandle_)
            ecl_kw_free(ertHandle_);

        name_ = name;
        ertHandle_ = ecl_kw_alloc(name.c_str(), data.size(), ertType_());

        // fill ERT object with values
        T* target = static_cast<T*>(ecl_kw_get_ptr(ertHandle()));
        for (unsigned i = 0; i < data.size(); ++i)
            target[i] = static_cast<T>(data[i]);

        Valgrind::CheckDefined(target, data.size());
#endif
    }

    // special case for string keywords
    void set(const std::string name, const std::vector<const char*>& data)
    {
#if HAVE_ERT
        if(ertHandle_)
            ecl_kw_free(ertHandle_);

        name_ = name;
        ertHandle_ = ecl_kw_alloc(name.c_str(), data.size(), ertType_());

        // fill ERT object with values
        for (unsigned i = 0; i < data.size(); ++i)
            ecl_kw_iset_char_ptr(ertHandle_, i, data[i]);
#endif
    }

    const std::string &name() const
    { return name_; }

    ErtHandleType *ertHandle() const
    { return ertHandle_; }

private:
#if HAVE_ERT
    static ecl_type_enum ertType_()
    {
        if (std::is_same<T, float>::value)
        { return ECL_FLOAT_TYPE; }
        if (std::is_same<T, double>::value)
        { return ECL_DOUBLE_TYPE; }
        if (std::is_same<T, int>::value)
        { return ECL_INT_TYPE; }
        if (std::is_same<T, const char*>::value)
        { return ECL_CHAR_TYPE; }

        OPM_THROW(std::logic_error,
                  "Unhandled type for data elements in ErtKeyword");
    }
#endif

    std::string name_;
    ErtHandleType *ertHandle_;
};

/*!
 * \ingroup EclBlackOilSimulator
 *
 * \brief This is a smart pointer class for ERT's ecl_grid_type
 *        structure.
 */
class ErtGrid
{

public:
#if HAVE_ERT
    typedef ecl_grid_type ErtHandleType;
#else
    typedef int ErtHandleType;
#endif

    ErtGrid(const ErtGrid& ) = delete;

    /*!
     * \brief Create an ERT grid based an Opm::EclipseGrid.
     */
    template <class DeckUnits>
    ErtGrid(Opm::EclipseGridConstPtr eclGrid, const DeckUnits& deckUnits)
    {
#if HAVE_ERT
        std::vector<double> mapaxesData;
        std::vector<double> coordData;
        std::vector<double> zcornData;
        std::vector<int> actnumData;

        eclGrid->exportMAPAXES(mapaxesData);
        eclGrid->exportCOORD(coordData);
        eclGrid->exportZCORN(zcornData);
        eclGrid->exportACTNUM(actnumData);

        // conversion to deck units
        deckUnits.siToDeck(mapaxesData, DeckUnits::length);
        deckUnits.siToDeck(coordData, DeckUnits::length);
        deckUnits.siToDeck(zcornData, DeckUnits::length);

        ErtKeyword<float> mapaxesKeyword("MAPAXES", mapaxesData);
        ErtKeyword<float> coordKeyword("COORD", coordData);
        ErtKeyword<float> zcornKeyword("ZCORN", zcornData);
        ErtKeyword<int> actnumKeyword("ACTNUM", actnumData);

        ertHandle_ = ecl_grid_alloc_GRDECL_kw(eclGrid->getNX(),
                                              eclGrid->getNY(),
                                              eclGrid->getNZ(),
                                              zcornKeyword.ertHandle(),
                                              coordKeyword.ertHandle(),
                                              actnumKeyword.ertHandle(),
                                              mapaxesData.size()?mapaxesKeyword.ertHandle():NULL);
#endif // HAVE_ERT && HAVE_DUNE_CORNERPOINT
    }

    ~ErtGrid()
    {
#if HAVE_ERT
        ecl_grid_free(ertHandle_);
#endif
    }


    /*!
     * \brief Save the grid to an .EGRID file.
     */
    void write(const std::string& fileName, int reportStepIdx)
    {
#if HAVE_ERT
        ecl_grid_fwrite_EGRID(ertHandle(), fileName.c_str());
#endif
    }

    ErtHandleType *ertHandle() const
    { return ertHandle_; }

private:
    ErtHandleType *ertHandle_;
};

/*!
 * \ingroup EclBlackOilSimulator
 *
 * \brief This is a smart pointer class for ERT's ecl_rst_file_type
 *        structure.
 */
class ErtRestartFile
{
    static const int numIwellItemsPerWell = 11;
    static const int numZwelStringsPerWell = 3;
    static const int numIconItemsPerConnection = 15;

public:
    ErtRestartFile(const ErtRestartFile &) = delete;

    template <class Simulator>
    ErtRestartFile(const Simulator &simulator, int reportStepIdx)
    {
        std::string caseName = getErtCaseName__(simulator.gridManager());

        restartFileName_ = ecl_util_alloc_filename("./",
                                                   caseName.c_str(),
                                                   /*type=*/ECL_UNIFIED_RESTART_FILE,
                                                   /*writeFormatedOutput=*/false,
                                                   reportStepIdx);

        if (reportStepIdx == 0)
            restartFileHandle_ = ecl_rst_file_open_write(restartFileName_);
        else
            restartFileHandle_ = ecl_rst_file_open_append(restartFileName_);
    }

    ~ErtRestartFile()
    {
        ecl_rst_file_close(restartFileHandle_);
        free(restartFileName_);
    }

    /*!
     * \brief Write the header for the current report step.
     *
     * This data structure contains how much time has elapsed since the beginning of the
     * simulation and the locations and names of the wells as well as which cells are
     * pierced by them.
     */
    template <class Simulator>
    void writeHeader(const Simulator &simulator, int reportStepIdx)
    {
        const auto eclGrid = getEclGrid__(simulator.gridManager());
        const auto eclState = simulator.gridManager().eclState();
        const auto eclSchedule = eclState->getSchedule();

        double secondsElapsed = simulator.time() + simulator.timeStepSize();
        double daysElapsed = secondsElapsed/(24*60*60);

        ecl_rsthead_type rstHeader;
        rstHeader.sim_time = simulator.startTime() + secondsElapsed;
        rstHeader.nactive = eclGrid->getNumActive();
        rstHeader.nx = eclGrid->getNX();
        rstHeader.ny = eclGrid->getNY();
        rstHeader.nz = eclGrid->getNZ();
        rstHeader.nwells = eclSchedule->numWells(reportStepIdx);
        rstHeader.niwelz = numIwellItemsPerWell;
        rstHeader.nzwelz = numZwelStringsPerWell;
        rstHeader.niconz = numIconItemsPerConnection;
        rstHeader.phase_sum = ECL_OIL_PHASE | ECL_WATER_PHASE | ECL_GAS_PHASE;
        rstHeader.ncwmax = eclSchedule->getMaxNumCompletionsForWells(reportStepIdx);
        rstHeader.sim_days = daysElapsed;
        ecl_rst_file_fwrite_header(restartFileHandle_, reportStepIdx, &rstHeader);

        // well information
        std::vector<int> iconData;
        std::vector<int> iwelData;
        std::vector<const char*> zwelData;

        const auto& eclWells = eclSchedule->getWells(reportStepIdx);
        auto eclWellIt = eclWells.begin();
        const auto& eclWellEndIt = eclWells.end();
        for (; eclWellIt != eclWellEndIt; ++eclWellIt) {
            appendIwelData_(iwelData, *eclWellIt, reportStepIdx);
            appendZwelData_(zwelData, *eclWellIt, reportStepIdx);
            appendIconData_(iconData, *eclWellIt, reportStepIdx, rstHeader.ncwmax);
        }

        ErtKeyword<int> iwelKeyword(IWEL_KW, iwelData);
        ErtKeyword<const char*> zwelKeyword(ZWEL_KW, zwelData);
        ErtKeyword<int> iconKeyword(ICON_KW, iconData);

        ecl_rst_file_add_kw(restartFileHandle_, iwelKeyword.ertHandle());
        ecl_rst_file_add_kw(restartFileHandle_, zwelKeyword.ertHandle());
        ecl_rst_file_add_kw(restartFileHandle_, iconKeyword.ertHandle());
    }

    ecl_rst_file_type *ertHandle() const
    { return restartFileHandle_; }

private:
    void appendIwelData_(std::vector<int>& iwelData,
                         Opm::WellConstPtr eclWell,
                         size_t reportStepIdx) const
    {
        int offset = iwelData.size();

        iwelData.resize(iwelData.size() + numIwellItemsPerWell, 0);

        Opm::CompletionSetConstPtr completionSet = eclWell->getCompletions(reportStepIdx);
        iwelData[offset + IWEL_HEADI_ITEM] = eclWell->getHeadI() + 1;
        iwelData[offset + IWEL_HEADJ_ITEM] = eclWell->getHeadJ() + 1;
        iwelData[offset + IWEL_CONNECTIONS_ITEM] = completionSet->size();
        iwelData[offset + IWEL_GROUP_ITEM] = 1; // currently we implement only a single group
        iwelData[offset + IWEL_TYPE_ITEM] = ertWellType_(eclWell, reportStepIdx);
        iwelData[offset + IWEL_STATUS_ITEM] = ertWellStatus_(eclWell, reportStepIdx);

        assert(iwelData.size() % numIwellItemsPerWell == 0);
    }

    void appendZwelData_(std::vector<const char*>& zwelData,
                         Opm::WellConstPtr eclWell,
                         size_t /*reportStepIdx*/) const
    {
        zwelData.push_back(eclWell->name().c_str());
        zwelData.push_back("");
        zwelData.push_back("");

        assert(zwelData.size() % numZwelStringsPerWell == 0);
    }

    void appendIconData_(std::vector<int>& iconData,
                         Opm::WellConstPtr eclWell,
                         size_t reportStepIdx,
                         int maxNumConnections) const
    {
        int offset = iconData.size();

        iconData.resize(iconData.size() + maxNumConnections*numIconItemsPerConnection, 0);
        Opm::CompletionSetConstPtr completionsSet = eclWell->getCompletions(reportStepIdx);
        for (size_t i = 0; i < completionsSet->size(); ++i) {
            Opm::CompletionConstPtr completion = completionsSet->get(i);

            iconData[offset + ICON_IC_ITEM] = 1;
            iconData[offset + ICON_I_ITEM] = completion->getI() + 1;
            iconData[offset + ICON_J_ITEM] = completion->getJ() + 1;
            iconData[offset + ICON_K_ITEM] = completion->getK() + 1;
            iconData[offset + ICON_STATUS_ITEM] = (completion->getState() == Opm::WellCompletion::OPEN)?1:0;

            int eclDirection;
            switch (completion->getDirection()) {
            case Opm::WellCompletion::X:
                eclDirection = 1;
                break;

            case Opm::WellCompletion::Y:
                eclDirection = 2;
                break;

            case Opm::WellCompletion::Z:
                eclDirection = 3;
                break;

            default:
                OPM_THROW(std::logic_error, "Encountered unimplemented completion direction.");
            }
            iconData[offset + ICON_STATUS_ITEM] = eclDirection;

            offset += numIconItemsPerConnection;
        }

        assert(iconData.size() % numIconItemsPerConnection == 0);
    }

    int ertWellType_(Opm::WellConstPtr eclWell, size_t reportStepIdx) const
    {
        int ertWellType = IWEL_UNDOCUMENTED_ZERO;

        if (eclWell->isProducer(reportStepIdx))
            ertWellType = IWEL_PRODUCER;
        else {
            switch (eclWell->getInjectionProperties(reportStepIdx).injectorType) {
            case Opm::WellInjector::WATER:
                ertWellType = IWEL_WATER_INJECTOR;
                break;

            case Opm::WellInjector::GAS:
                ertWellType = IWEL_GAS_INJECTOR;
                break;

            case Opm::WellInjector::OIL :
                ertWellType = IWEL_OIL_INJECTOR;
                break;

            default:
                // oops!
                ertWellType = IWEL_UNDOCUMENTED_ZERO;
            }
        }

        return ertWellType;
    }

    int ertWellStatus_(Opm::WellConstPtr eclWell, size_t reportStepIdx) const
    {
        int ertWellStatus;

        if (eclWell->getStatus(reportStepIdx) == Opm::WellCommon::OPEN)
            ertWellStatus = 1;
        else
            ertWellStatus = 0; // shut or closed...

        return ertWellStatus;
    }

    char *restartFileName_;
    ecl_rst_file_type *restartFileHandle_;
};

/**
 * \ingroup EclBlackOilSimulator
 *
 * \brief The ErtSolution class wraps the actions that must be done to the
 *        restart file while writing solution variables; it is not a handle on
 *        its own.
 */
class ErtSolution
{
public:
    ErtSolution(const ErtSolution&) = delete;

    ErtSolution(ErtRestartFile &restartHandle)
        : restartHandle_(&restartHandle)
    {  ecl_rst_file_start_solution(restartHandle_->ertHandle()); }

    ~ErtSolution()
    { ecl_rst_file_end_solution(restartHandle_->ertHandle()); }

    template <typename T>
    void add(std::shared_ptr<const ErtKeyword<T>> ertKeyword)
    {
        attachedKeywords_.push_back(ertKeyword);
        ecl_rst_file_add_kw(restartHandle_->ertHandle(), ertKeyword->ertHandle());
    }

    ecl_rst_file_type *ertHandle() const
    { return restartHandle_->ertHandle(); }

private:
    ErtRestartFile *restartHandle_;
    std::list<std::shared_ptr<const ErtBaseKeyword>> attachedKeywords_;
};

/**
 * \ingroup EclBlackOilSimulator
 *
 * \brief The ErtSummary class wraps the actions that must be done to write ECL
 *        summary file.
 *
 * These files log the well performance, etc...
 */
template <class TypeTag>
class ErtSummary
{
    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;

    typedef EclWellManager<TypeTag> WellManager;

public:
    ErtSummary(const ErtSummary&) = delete;

    ErtSummary(const Simulator& simulator)
    {
        const auto& gridManager = simulator.gridManager();
        const auto& eclGrid = gridManager.eclGrid();
        auto timeMap = gridManager.schedule()->getTimeMap();

        std::string caseName = getErtCaseName__(gridManager);

        // the correct start time has not yet been set in the
        // simulator, so we extract it from the ECL deck...
        tm curTime = boost::posix_time::to_tm(timeMap->getStartTime(/*timeStepIdx=*/0));
        double startTime = std::mktime(&curTime);

        ertHandle_ = ecl_sum_alloc_writer(caseName.c_str(),
                                          /*formatted=*/false,
                                          /*unified=*/true,
                                          /*joinString=*/":",
                                          startTime,
                                          /*timeIsInDays=*/false,
                                          eclGrid->getNX(),
                                          eclGrid->getNY(),
                                          eclGrid->getNZ());
    }

    ~ErtSummary()
    { ecl_sum_free(ertHandle_); }

    // add all wells in the well manager to the summary output and
    // write the result.
    void writeTimeStep(const WellManager& wellManager)
    { }

    ecl_sum_type *ertHandle() const
    { return ertHandle_; }

private:
    ecl_sum_type *ertHandle_;
};

/**
 * \ingroup EclBlackOilSimulator
 *
 * \brief The ErtSummary class wraps the ERT handles which are required to write a single
 *        time step to the ECL summary file.
 */
template <class TypeTag>
class ErtSummaryTimeStep
{
public:
    ErtSummaryTimeStep(ErtSummary<TypeTag>& summaryHandle,
                       double timeInSeconds,
                       int reportStepIdx)
    {
        double timeInDays = timeInSeconds / (24*60*60);
        ertHandle_ = ecl_sum_add_tstep(summaryHandle.ertHandle(), reportStepIdx, timeInDays);
    }

    // no destructor in this class as ERT takes care of freeing the
    // handle as part of freeing the summary file handle!

    ecl_sum_tstep_type *ertHandle() const
    { return ertHandle_; };

private:
    ecl_sum_tstep_type *ertHandle_;
};

} // namespace Ewoms

#endif // HAVE_ERT

#endif // EWOMS_ERT_WRAPPERS_HH
