from hoomd import _hoomd
from hoomd.operation import _Operation
from hoomd.custom import (
    _CustomOperation, _InternalCustomOperation, Action)
from hoomd.operation import _Tuner


class _TunerProperty:
    @property
    def tuner(self):
        return self._action

    @tuner.setter
    def tuner(self, tuner):
        if isinstance(tuner, Action):
            self._action = tuner
        else:
            raise ValueError(
                "updater must be an instance of hoomd.custom.Action")


class CustomTuner(_CustomOperation, _TunerProperty, _Tuner):
    """Tuner wrapper for `hoomd.custom.Action` objects.

    For usage see `hoomd.custom._CustomOperation`.
    """
    _cpp_list_name = 'tuners'
    _cpp_class_name = 'PythonTuner'

    def _attach(self):
        self._cpp_obj = getattr(_hoomd, self._cpp_class_name)(
            self._simulation.state._cpp_sys_def, self.trigger, self._action)
        self._action._attach(self._simulation)
        _Operation._attach(self)


class _InternalCustomTuner(
        _InternalCustomOperation, _TunerProperty, _Tuner):
    _cpp_list_name = 'tuners'
    _cpp_class_name = 'PythonTuner'

    def _attach(self):
        self._cpp_obj = getattr(_hoomd, self._cpp_class_name)(
            self._simulation.state._cpp_sys_def, self.trigger, self._action)
        self._action._attach(self._simulation)
        _Operation._attach(self)
