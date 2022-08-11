from transitions import Machine
from transitions.extensions import GraphMachine
from copy import copy
from ast import literal_eval


class PureStateMachine:
    def __init__(self, name: str, states: tuple = (), transitions: list = (), path=None):
        """ Base state machine class

        :param name:str=(): Define the name of the machine
        :param states:tuple=(): Define the states of the machine
        :param transitions:list=(): Define the transitions of the graph
        :param path=None: Pass the path to the rulebook
        """
        self.gsm = copy(self)
        self.name = name
        """ name of state machine """
        if path is not None:
            states, transitions = self.read_rulebook(path)

        self.state_init = self.name.upper() + "_INIT"
        """ default init FSM state"""
        self.state_aborted = self.name.upper() + "_ABORTED"
        """ default aborted FSM state"""
        self.state_done = self.name.upper() + "_DONE"
        """ default done FSM state"""
        self.transition_start = self.name.lower() + "_start"
        """ default start FSM transition"""
        self.transition_end = self.name.lower() + "_end"
        """ default end FSM transition"""
        self.default_states = (self.state_init,
                               self.state_aborted, self.state_done)
        """ default states for FSM """
        self.default_transitions = [
            [self.transition_end, '*', self.state_done]
        ]
        """ default transitions for FSM """

        states = self.default_states + states
        transitions = self.default_transitions + transitions

        self.g_machine = GraphMachine(
            model=self.gsm, states=states, transitions=transitions, initial=self.state_init)
        """ pytransitions graph state machine """
        self.machine = Machine(model=self, states=states, transitions=transitions,
                               initial=self.state_init, auto_transitions=False)
        """ pytransitions state machine """
        self.verbose = True

    @staticmethod
    def callback_wrapper(userdata: dict = None, external_cb=None):
        """
        The callback_wrapper function is a helper function that wraps the user-defined callback function.
        It is used to handle the following:
            - The return value of the callback_wrapper is always a dictionary with two keys: 'status' and 'data'.
              If there was an error, status will be set to False and data will contain an error message.
              Otherwise, status will be True and data contains your original return value from your callback function.

        :param userdata:dict=None: Pass in data to the callback function
        :param external_cb=None: Pass in a callback function that has been passed to the state machine
        :return: A function that takes in a userdata and an external_cb argument
        """
        if not callable(external_cb):
            raise TypeError(
                "Callable function should be passed to callback_wrapper")
        external_cb(userdata)

    def next_step(self):
        """
        The next_step function is the default variant of next step. It should be overridden to do complex callbacks

        :param self: Access the attributes of the class
        :return: The trigger that is associated with the current state
        """
        if self.verbose:
            print(f"DEBUG: current state of abstract machine is {self.state}")
            print(
                f"DEBUG: doing the transition {self.machine.get_triggers(self.state)[0]}")

        self.trigger(self.machine.get_triggers(self.state)[0],
                     {'state_name': self.state})

    @staticmethod
    def read_rulebook(path):
        """
        The read_rulebook function reads a rulebook and returns the states, transitions,
        and initial state. The function takes one argument: path to the file containing
        the information about the states and transitions.

        :param path: Specify the location of the rulebook
        :return: A list of dictionaries
        
        """
        """rules for writings rulebooks should be specified"""
        states = None
        tr_list = []
        with open(path) as f:
            for line in f:
                if line[0] == '(':
                    states = literal_eval(line)
                elif line[0] == '{':
                    tr_list.append(literal_eval(line))
        return states, tr_list

    def set_verbose(self, verbose):
        """
        The set_verbose function is used to set the verbose attribute of FSM_simple class.
        It is typically called to allow callers to control whether or not
        detailed output is printed by that module.

        :param verbose: Determine whether the function will print out a message
        :return: The value of the verbose parameter
        
        """
        if verbose:
            self.verbose = True
        else:
            self.verbose = False

    def describe(self):
        """
        The describe function draws a state diagram of machine
        """
        self.gsm.get_graph().draw(f"{self.name}_machine_diagram.png")

    def set_state(self, state):
        """
        The set_state function sets the state of the FSM.

        :param state: Set the valid state of the FSM
        
        """
        self.machine.set_state(state)

    def run(self):
        """
        The run function is the main function of the state machine. It is called
        when a state machine is started, and it continues to execute until it reaches
        the 'done' or 'aborted' states. The run function executes one step of the
        state machine at a time, where each step performs some operation on the robot
        and then advances to its next state based on which transition conditions are met.

        :return: 1 if the state is done
        """
        current_state = self.state
        while current_state != self.state_done and current_state != self.state_aborted:
            """conditional transitions are handled in next_step"""

            self.next_step()
            current_state = self.state
            # print('\n==== STEP IS OVER ====\n')

        if current_state == self.state_done:
            return 1
        elif current_state == self.state_aborted:
            return 0
        else:
            raise TypeError('Machine final state is not "done" or "aborted"')

    def add_state(self, states, **kwargs):
        self.machine.add_states(states, **kwargs)

    def add_transitions(self, transitions):
        self.machine.add_transitions(transitions)
