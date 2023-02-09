module.exports = {
	apps : [{
		name		:"Gaugemancy",
		script	:"cd /home/raspberry/openFrameworks/apps/myApps/Gaugemancy/bin &&./Gaugemancy",
		autorestart: false,
		exec_interpreter: "none",
		exec_mode : "fork_mode"
	}]
}
