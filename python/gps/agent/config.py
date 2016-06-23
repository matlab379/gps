""" Default configuration and hyperparameters for agent objects. """
import logging

import numpy as np


LOGGER = logging.getLogger(__name__)


# Agent
AGENT = {
    'dH': 0,
    'x0var': 0,
    'noisy_body_idx': np.array([]),
    'noisy_body_var': np.array([]),
    'pos_body_idx': np.array([]),
    'pos_body_offset': np.array([]),
    'smooth_noise': True,
    'smooth_noise_var': 2.0,
    'smooth_noise_renormalize': True,
}


try:
    import rospkg

    import roslib

    roslib.load_manifest('gps_agent_pkg')

    # AgentROS
    AGENT_ROS = {
        #TODO: It might be worth putting this in JSON/yaml format so C++
        #      can read it.
        'trial_command_topic': 'gps_controller_trial_command',
        'reset_command_topic': 'gps_controller_position_command',
        'relax_command_topic': 'gps_controller_relax_command',
        'data_request_topic': 'gps_controller_data_request',
        'sample_result_topic': 'gps_controller_report',
        'trial_timeout': 99999999999999999999999999999920,  # Give this many seconds for a trial.
        'reset_conditions': [],  # Defines reset modes + positions for
                                 # trial and auxiliary arms.
        'frequency': 20,
        'end_effector_points': np.array([]),
        #TODO: Actually pass in low gains and high gains and use both
        #      for the position controller.
        'pid_params': np.array([
            1.163, 0.041, 0.13, 0.01,
            6.66, 0.009, 0.3, 0.01,
            3.3, 0.01, 0.032, 0.001,
            3.4, 0.01, 0.0312, 0.001,
            3.0, 0.001, 0.01, 0.041,
            5.0, 0.01, 0.041, 0.001,
            8.0, 1.0, 0.1, 0.01
        ]),
    }
except ImportError as e:
    AGENT_ROS = {}
    LOGGER.debug('No ROS enabled: %s', e)
except rospkg.common.ResourceNotFound as e:
    AGENT_ROS = {}
    LOGGER.debug('No gps_agent_pkg: %s', e)


# AgentMuJoCo
AGENT_MUJOCO = {
    'substeps': 1,
}

AGENT_BOX2D = {
}
