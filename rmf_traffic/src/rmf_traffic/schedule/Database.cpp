/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "ViewerInternal.hpp"

#include "../detail/internal_bidirectional_iterator.hpp"

#include <rmf_traffic/schedule/Database.hpp>

#include <algorithm>

namespace rmf_traffic {
namespace schedule {

//==============================================================================
class Database::Change::Implementation
{
public:

  Mode mode;
  Insert insert;
  Interrupt interrupt;
  Delay delay;
  Replace replace;
  Erase erase;
  Cull cull;

  std::size_t id;

  static Change make(const Mode mode, const std::size_t id)
  {
    Change change;
    change._pimpl->mode = mode;
    change._pimpl->id = id;

    return change;
  }

  static Change make_insert_ref(
      const Trajectory* trajectory,
      std::size_t id);

  static Change make_interrupt_ref(
      std::size_t original_id,
      const Trajectory* interruption_trajectory,
      Duration delay,
      std::size_t id);

  static Change make_replace_ref(
      std::size_t original_id,
      const Trajectory* trajectory,
      std::size_t id);
};

namespace {
//==============================================================================
// TODO(MXG): Consider generalizing this class using templates if it ends up
// being useful in any other context.
class DeepOrShallowTrajectory
{
public:

  const Trajectory* get() const
  {
    if(is_deep)
      return deep.get();

    return shallow;
  }

  rmf_utils::impl_ptr<const Trajectory> deep;
  const Trajectory* shallow = nullptr;
  bool is_deep = true;

};

//==============================================================================
DeepOrShallowTrajectory make_deep(const Trajectory* const deep)
{
  DeepOrShallowTrajectory traj;
  traj.is_deep = true;
  if(deep)
    traj.deep = rmf_utils::make_impl<const Trajectory>(*deep);

  return traj;
}

DeepOrShallowTrajectory make_shallow(const Trajectory* const shallow)
{
  DeepOrShallowTrajectory traj;
  traj.is_deep = false;
  traj.shallow = shallow;

  return traj;
}

} // anonymous namespace

//==============================================================================
class Database::Change::Insert::Implementation
{
public:

  /// The trajectory that was inserted
  DeepOrShallowTrajectory trajectory;

  /// This is used to create a Change whose lifetime is not tied to the Database
  /// instance that constructed it.
  static Insert make_copy(const Trajectory* const trajectory)
  {
    Insert result;

    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{make_deep(trajectory)});

    return result;
  }

  /// This is used by Database instances to create a Change without the overhead
  /// of copying a Trajectory instance. The Change's lifetime will be tied to
  /// the lifetime of the Database object.
  static Insert make_ref(const Trajectory* const trajectory)
  {
    Insert result;

    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{make_shallow(trajectory)});

    return result;
  }
};

//==============================================================================
Database::Change::Insert::Insert()
{
  // Do nothing
}

//==============================================================================
Database::Change::Change()
  : _pimpl(rmf_utils::make_impl<Implementation>())
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_insert(
    const Trajectory* const trajectory,
    std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Insert, id);
  change._pimpl->insert = Insert::Implementation::make_copy(trajectory);

  return change;
}

//==============================================================================
auto Database::Change::Implementation::make_insert_ref(
    const Trajectory* const trajectory,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Insert, id);
  change._pimpl->insert = Insert::Implementation::make_ref(trajectory);

  return change;
}

//==============================================================================
class Database::Change::Interrupt::Implementation
{
public:

  DeepOrShallowTrajectory trajectory;
  std::size_t original_id;
  Duration delay;

  template<typename... Args>
  static Interrupt make_copy(
      const Trajectory* const trajectory,
      Args&&... args)
  {
    Interrupt result;

    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{make_deep(trajectory), std::forward<Args>(args)...});

    return result;
  }

  template<typename... Args>
  static Interrupt make_ref(
      const Trajectory* const trajectory,
      Args&&... args)
  {
    Interrupt result;

    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{make_shallow(trajectory),
                         std::forward<Args>(args)...});

    return result;
  }

};

//==============================================================================
Database::Change::Interrupt::Interrupt()
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_interrupt(
    std::size_t original_id,
    const Trajectory* const interruption_trajectory,
    Duration delay,
    std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Interrupt, id);
  change._pimpl->interrupt = Interrupt::Implementation::make_copy(
        interruption_trajectory, original_id, delay);
  return change;
}

//==============================================================================
auto Database::Change::Implementation::make_interrupt_ref(
    const std::size_t original_id,
    const Trajectory* const interruption_trajectory,
    const Duration delay,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Interrupt, id);
  change._pimpl->interrupt = Interrupt::Implementation::make_ref(
        interruption_trajectory, original_id, delay);
  return change;
}

//==============================================================================
class Database::Change::Delay::Implementation
{
public:

  std::size_t original_id;
  Time from;
  Duration delay;

  template<typename... Args>
  static Delay make(Args&&... args)
  {
    Delay result;
    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{std::forward<Args>(args)...});
    return result;
  }

};

//==============================================================================
Database::Change::Delay::Delay()
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_delay(
    std::size_t original_id,
    Time from,
    Duration delay,
    std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Delay, id);
  change._pimpl->delay = Delay::Implementation::make(
        original_id, from, delay);
  return change;
}

//==============================================================================
class Database::Change::Replace::Implementation
{
public:

  std::size_t original_id;
  DeepOrShallowTrajectory trajectory;

  static Replace make_copy(
      const std::size_t original_id,
      const Trajectory* const trajectory)
  {
    Replace result;
    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{original_id, make_deep(trajectory)});
    return result;
  }

  static Replace make_ref(
      const std::size_t original_id,
      const Trajectory* const trajectory)
  {
    Replace result;
    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{original_id, make_shallow(trajectory)});
    return result;
  }

};

//==============================================================================
Database::Change::Replace::Replace()
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_replace(
    const std::size_t original_id,
    const Trajectory* const trajectory,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Replace, id);
  change._pimpl->replace = Replace::Implementation::make_copy(
        original_id, trajectory);
  return change;
}

//==============================================================================
auto Database::Change::Implementation::make_replace_ref(
    const std::size_t original_id,
    const Trajectory* const trajectory,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Replace, id);
  change._pimpl->replace = Replace::Implementation::make_ref(
        original_id, trajectory);
  return change;
}

//==============================================================================
class Database::Change::Erase::Implementation
{
public:

  std::size_t original_id;

  static Erase make(const std::size_t original_id)
  {
    Erase result;
    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{original_id});
    return result;
  }

};

//==============================================================================
Database::Change::Erase::Erase()
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_erase(
    const std::size_t original_id,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Erase, id);
  change._pimpl->erase = Erase::Implementation::make(original_id);
  return change;
}

//==============================================================================
class Database::Change::Cull::Implementation
{
public:

  std::vector<std::size_t> culled;

  static Cull make(std::vector<std::size_t> _culled)
  {
    Cull result;
    result._pimpl = rmf_utils::make_impl<Implementation>(
          Implementation{std::move(_culled)});
    return result;
  }

};

//==============================================================================
Database::Change::Cull::Cull()
{
  // Do nothing
}

//==============================================================================
auto Database::Change::make_cull(
    std::vector<std::size_t> culled,
    const std::size_t id) -> Change
{
  Change change = Implementation::make(Mode::Cull, id);
  change._pimpl->cull = Cull::Implementation::make(std::move(culled));
  return change;
}

//==============================================================================
const Trajectory* Database::Change::Insert::trajectory() const
{
  return _pimpl->trajectory.get();
}

//==============================================================================
std::size_t Database::Change::Interrupt::original_id() const
{
  return _pimpl->original_id;
}

//==============================================================================
const Trajectory* Database::Change::Interrupt::interruption() const
{
  return _pimpl->trajectory.get();
}

//==============================================================================
Duration Database::Change::Interrupt::delay() const
{
  return _pimpl->delay;
}

//==============================================================================
std::size_t Database::Change::Delay::original_id() const
{
  return _pimpl->original_id;
}

//==============================================================================
Time Database::Change::Delay::from() const
{
  return _pimpl->from;
}

//==============================================================================
Duration Database::Change::Delay::duration() const
{
  return _pimpl->delay;
}

//==============================================================================
std::size_t Database::Change::Replace::original_id() const
{
  return _pimpl->original_id;
}

//==============================================================================
const Trajectory* Database::Change::Replace::trajectory() const
{
  return _pimpl->trajectory.get();
}

//==============================================================================
std::size_t Database::Change::Erase::original_id() const
{
  return _pimpl->original_id;
}

//==============================================================================
const std::vector<std::size_t>& Database::Change::Cull::culled_ids() const
{
  return _pimpl->culled;
}

//==============================================================================
auto Database::Change::get_mode() const -> Mode
{
  return _pimpl->mode;
}

//==============================================================================
std::size_t Database::Change::id() const
{
  return _pimpl->id;
}

//==============================================================================
auto Database::Change::insert() const -> const Insert*
{
  if(Mode::Insert == _pimpl->mode)
    return &_pimpl->insert;

  return nullptr;
}

//==============================================================================
auto Database::Change::interrupt() const -> const Interrupt*
{
  if(Mode::Interrupt == _pimpl->mode)
    return &_pimpl->interrupt;

  return nullptr;
}

//==============================================================================
auto Database::Change::delay() const -> const Delay*
{
  if(Mode::Delay == _pimpl->mode)
    return &_pimpl->delay;

  return nullptr;
}

//==============================================================================
auto Database::Change::replace() const -> const Replace*
{
  if(Mode::Replace == _pimpl->mode)
    return &_pimpl->replace;

  return nullptr;
}

//==============================================================================
auto Database::Change::erase() const -> const Erase*
{
  if(Mode::Erase == _pimpl->mode)
    return &_pimpl->erase;

  return nullptr;
}

//==============================================================================
auto Database::Change::cull() const -> const Cull*
{
  if(Mode::Cull == _pimpl->mode)
    return &_pimpl->cull;

  return nullptr;
}

//==============================================================================
class Database::Patch::Implementation
{
public:

  std::vector<Change> changes;

  std::size_t latest_version;

  Implementation()
  {
    // Do nothing
  }

  Implementation(std::vector<Change> _changes, std::size_t _latest_version)
    : changes(std::move(_changes)),
      latest_version(_latest_version)
  {
    // Sort the changes to make sure they get applied in the correct order
    std::sort(changes.begin(), changes.end(),
              [](const Change& c1, const Change& c2) {
      return c1.id() < c2.id();
    });
  }

};

//==============================================================================
class Database::Patch::IterImpl
{
public:

  std::vector<Change>::const_iterator iter;

};

//==============================================================================
Database::Patch::Patch(std::vector<Change> changes, std::size_t latest_version)
  : _pimpl(rmf_utils::make_impl<Implementation>(
             std::move(changes), latest_version))
{
  // Do nothing
}

//==============================================================================
auto Database::Patch::begin() const -> const_iterator
{
  return const_iterator(IterImpl{_pimpl->changes.begin()});
}

//==============================================================================
auto Database::Patch::end() const -> const_iterator
{
  return const_iterator(IterImpl{_pimpl->changes.end()});
}

//==============================================================================
std::size_t Database::Patch::size() const
{
  return _pimpl->changes.size();
}

//==============================================================================
std::size_t Database::Patch::latest_version() const
{
  return _pimpl->latest_version;
}

//==============================================================================
Database::Patch::Patch()
{
  // Do nothing
}

namespace internal {
//==============================================================================
void ChangeRelevanceInspector::after(const std::size_t* _after)
{
  after_version = _after;
}

//==============================================================================
void ChangeRelevanceInspector::reserve(std::size_t size)
{
  relevant_changes.reserve(size);
}

//==============================================================================
namespace {

ConstEntryPtr get_last_known_ancestor(
    ConstEntryPtr from, const std::size_t last_known_version)
{
  ConstEntryPtr check = from;
  while(check && last_known_version < check->version)
    check = check->succeeds;

  return check;
}

} // anonymous namespace

//==============================================================================
void ChangeRelevanceInspector::inspect(
    const ConstEntryPtr& entry,
    const std::function<bool(const ConstEntryPtr&)>& relevant)
{
  if(entry->succeeded_by)
    return;

  if(after_version && entry->version <= *after_version)
    return;

  const bool needed = relevant(entry);

  if(needed)
  {
    // Check if this entry descends from an entry that the remote mirror does
    // not know about.
    ConstEntryPtr record_changes_from = nullptr;
    if(after_version)
    {
      const ConstEntryPtr check =
          get_last_known_ancestor(entry, *after_version);

      if(check)
      {
        if(relevant(check))
        {
          // The remote mirror already knows the lineage of this entry, so we
          // will transmit all of its changes from the last version that the
          // mirror knew about.
          record_changes_from = check;
        }
        else
        {
          // The remote mirror does not know the lineage of this entry, so we
          // will simply transmit the current entry as an insertion. We simply
          // leave record_changes_from as a nullptr.
        }
      }
      else
      {
        // The remote mirror does not know the lineage of this entry, so we
        // will simply transmit the current entry as an insertion. We simply
        // leave record_changes_from as a nullptr.
      }
    }

    if(record_changes_from)
    {
      ConstEntryPtr record = record_changes_from->succeeded_by;
      while(record)
      {
        relevant_changes.emplace_back(*record->change);
        record = record->succeeded_by;
      }
    }
    else
    {
      // We are not transmitting the chain of changes that led to this entry,
      // so just create an insertion for it and transmit that.
      relevant_changes.emplace_back(
            Database::Change::Implementation::make_insert_ref(
              &entry->trajectory, entry->version));
    }
  }
  else if(after_version)
  {
    // Figure out if this trajectory needs to be erased
    const ConstEntryPtr check = get_last_known_ancestor(entry, *after_version);
    if(check)
    {
      if(relevant(check))
      {
        // This trajectory is no longer relevant to the remote mirror, so we
        // will tell the remote mirror to erase it rather than continuing to
        // transmit its change history. If a later version of this trajectory
        // becomes relevant again, we will tell it to insert it at that time.
        relevant_changes.emplace_back(
              Database::Change::make_erase(check->version, entry->version));
      }
    }
  }
  else
  {
    // The remote mirror never knew about the lineage of this entry, so there's
    // no need to transmit any information about it at all.
  }
}

//==============================================================================
void ChangeRelevanceInspector::inspect(
    const ConstEntryPtr& entry,
    const rmf_traffic::internal::Spacetime& spacetime)
{
  inspect(entry, [&](const ConstEntryPtr& e) -> bool {
    return rmf_traffic::internal::detect_conflicts(
          e->trajectory, spacetime, nullptr);
  });
}

//==============================================================================
void ChangeRelevanceInspector::inspect(
    const ConstEntryPtr& entry,
    const Time* const lower_time_bound,
    const Time* const upper_time_bound)
{
  inspect(entry, [&](const ConstEntryPtr& e) -> bool {
    const Trajectory& trajectory = e->trajectory;
    assert(trajectory.start_time() != nullptr);

    if(lower_time_bound && *trajectory.finish_time() < *lower_time_bound)
      return false;

    if(upper_time_bound && *upper_time_bound < *trajectory.start_time())
      return false;

    return true;
  });
}

} // namespace internal

//==============================================================================
auto Database::changes(const Query& parameters) const -> Patch
{
  return Patch(_pimpl->inspect<internal::ChangeRelevanceInspector>(
                 parameters).relevant_changes, latest_version());
}

} // namespace schedule

namespace detail {

template class bidirectional_iterator<
    const schedule::Database::Change,
    schedule::Database::Patch::IterImpl,
    schedule::Database::Patch
>;

} // namespace detail

} // namespace rmf_traffic
